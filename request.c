/* vim: set tabstop=4:softtabstop=4:shiftwidth=4:noexpandtab */

/*
 * dnsbalancer - daemon to balance UDP DNS requests over DNS servers
 * Copyright (C) 2015-2016 Lanet Network
 * Programmed by Oleksandr Natalenko <o.natalenko@lanet.ua>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "contrib/xxhash/xxhash.h"

#include "request.h"

struct db_request_data db_make_request_data(ldns_pkt* _packet, int _forwarder_socket)
{
	struct db_request_data ret;
	ldns_rr* rr = NULL;
	ldns_rdf* owner = NULL;
	char* owner_str = NULL;

	pfcq_zero(&ret, sizeof(struct db_request_data));

	rr = ldns_rr_list_rr(ldns_pkt_question(_packet), 0);
	ret.rr_type = ldns_rr_get_type(rr);
	ret.rr_class = ldns_rr_get_class(rr);
	owner = ldns_rr_owner(rr);
	ldns_dname2canonical(owner);
	owner_str = ldns_rdf2str(owner);
	strncpy(ret.fqdn, owner_str, HOST_NAME_MAX);
	free(owner_str);
	ret.forwarder_socket = _forwarder_socket;
	ret.hash = XXH64((uint8_t*)&ret.rr_type, sizeof(ldns_rr_type), DB_HASH_SEED);
	ret.hash = XXH64((uint8_t*)&ret.rr_class, sizeof(ldns_rr_class), ret.hash);
	ret.hash = XXH64((uint8_t*)ret.fqdn, strlen(ret.fqdn), ret.hash);
	ret.hash = XXH64((uint8_t*)&ret.forwarder_socket, sizeof(int), ret.hash);

	return ret;
}

int db_compare_request_data(struct db_request_data _data1, struct db_request_data _data2)
{
	return
		_data1.hash == _data2.hash &&
		likely(
			_data1.rr_type == _data2.rr_type &&
			_data1.rr_class == _data2.rr_class &&
			_data1.forwarder_socket == _data2.forwarder_socket &&
			strncmp(_data1.fqdn, _data2.fqdn, HOST_NAME_MAX) == 0);
}

struct db_request* db_make_request(ldns_pkt* _packet, struct db_request_data _data, pfcq_net_address_t _address, size_t _forwarder_index)
{
	struct db_request* ret = NULL;

	ret = pfcq_alloc(sizeof(struct db_request));

	ret->original_id = ldns_pkt_id(_packet);
	ret->data = _data;
	ret->client_address = _address;
	if (unlikely(clock_gettime(CLOCK_REALTIME, &ret->ctime)))
		panic("clock_gettime");
	ret->forwarder_index = _forwarder_index;

	return ret;
}

uint16_t db_insert_request(struct db_request_list* _list, struct db_request* _request)
{
	uint16_t index = 0;
	uint32_t index_container = 0;

	if (unlikely(pthread_spin_lock(&_list->list_index_lock)))
		panic("pthread_spin_lock");
	index = _list->list_index;
	index_container = _list->list_index;
	index_container++;
	_list->list_index = (uint16_t)(index_container % UINT16_MAX);
	if (unlikely(pthread_spin_unlock(&_list->list_index_lock)))
		panic("pthread_spin_unlock");

	if (unlikely(pthread_mutex_lock(&_list->list[index].requests_lock)))
		panic("pthread_mutex_lock");
	TAILQ_INSERT_TAIL(&_list->list[index].requests, _request, tailq);
	_list->list[index].requests_count++;
	if (unlikely(pthread_mutex_unlock(&_list->list[index].requests_lock)))
		panic("pthread_mutex_unlock");

	if (unlikely(pthread_spin_lock(&_list->requests_count_lock)))
		panic("pthread_spin_lock");
	_list->requests_count++;
	if (unlikely(pthread_spin_unlock(&_list->requests_count_lock)))
		panic("pthread_spin_unlock");

	return index;
}

struct db_request* db_eject_request(struct db_request_list* _list, uint16_t _index, struct db_request_data _data)
{
	struct db_request* ret = NULL;

	struct db_request* current_request = NULL;
	if (unlikely(pthread_mutex_lock(&_list->list[_index].requests_lock)))
		panic("pthread_mutex_lock");
	TAILQ_FOREACH(current_request, &_list->list[_index].requests, tailq)
	{
		if (db_compare_request_data(current_request->data, _data))
		{
			ret = current_request;
			break;
		}
	}
	if (likely(ret))
	{
		TAILQ_REMOVE(&_list->list[_index].requests, ret, tailq);
		_list->list[_index].requests_count--;
		if (unlikely(pthread_spin_lock(&_list->requests_count_lock)))
			panic("pthread_spin_lock");
		_list->requests_count--;
		if (unlikely(pthread_spin_unlock(&_list->requests_count_lock)))
			panic("pthread_spin_unlock");
	}

	if (unlikely(pthread_mutex_unlock(&_list->list[_index].requests_lock)))
		panic("pthread_mutex_unlock");

	return ret;
}

void db_remove_request_unsafe(struct db_request_list* _list, uint16_t _index, struct db_request* _request)
{
	TAILQ_REMOVE(&_list->list[_index].requests, _request, tailq);
	pfcq_free(_request);
	_list->list[_index].requests_count--;
	if (unlikely(pthread_spin_lock(&_list->requests_count_lock)))
		panic("pthread_spin_lock");
	_list->requests_count--;
	if (unlikely(pthread_spin_unlock(&_list->requests_count_lock)))
		panic("pthread_spin_unlock");

	return;
}

