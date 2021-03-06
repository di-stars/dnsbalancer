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

#include "acl.h"

#include "contrib/xxhash/xxhash.h"

void db_acl_free_item(struct db_acl_item* _item)
{
	pfcq_free(_item->s_action_parameters);
	pfcq_free(_item->s_action);
	pfcq_free(_item->s_list);
	pfcq_free(_item->s_matcher);
	pfcq_free(_item->s_netmask);
	pfcq_free(_item->s_address);
	pfcq_free(_item->s_layer3);
	pthread_spin_destroy(&_item->hits_lock);
	pfcq_free(_item);

	return;
}

void db_acl_free_list_item(struct db_list_item* _item)
{
	pfcq_free(_item->s_name);
	pfcq_free(_item->s_fqdn);
	if (likely(_item->regex_compiled))
		regfree(&_item->regex);
	pfcq_free(_item);

	return;
}

enum db_acl_action db_check_query_acl(sa_family_t _layer3, pfcq_net_address_t* _address, struct db_request_data* _request_data, struct db_acl* _acl,
	void** _acl_data, size_t* _acl_data_length)
{
	enum db_acl_action ret = DB_ACL_ACTION_ALLOW;

	struct db_acl_item* current_acl_item = NULL;
	TAILQ_FOREACH(current_acl_item, _acl, tailq)
	{
		// Match IP address
		unsigned short int address_matched = 0;
		struct in6_addr anded6;
		switch (_layer3)
		{
			case PF_INET:
				address_matched = (unsigned short int)
					((_address->address4.sin_addr.s_addr & current_acl_item->netmask.address4.s_addr) == current_acl_item->address.address4.s_addr);
				break;
			case PF_INET6:
				for (size_t k = 0; k < 16; k++)
				{
					anded6.s6_addr[k] = (uint8_t)(_address->address6.sin6_addr.s6_addr[k] & current_acl_item->netmask.address6.s6_addr[k]);
					address_matched = (unsigned short int)(anded6.s6_addr[k] == current_acl_item->address.address6.s6_addr[k]);
					if (!address_matched)
						break;
				}
				break;
			default:
				panic("socket domain");
				break;
		}

		if (!address_matched)
			continue;

		// Match request
		uint64_t fqdn_hash = XXH64((uint8_t*)_request_data->fqdn, strlen(_request_data->fqdn), DB_HASH_SEED);
		unsigned short int matcher_matched = 0;
		struct db_list_item* current_list_item = NULL;
		TAILQ_FOREACH(current_list_item, &current_acl_item->list, tailq)
		{
			// Match RR type
			switch (current_list_item->rr_type)
			{
				case DB_ACL_RR_TYPE_ALL:
					// Match everything
					break;
				case DB_ACL_RR_TYPE_ANY:
					if (_request_data->rr_type != LDNS_RR_TYPE_ANY)
						continue;
					break;
				default:
					break;
			}

			// Match FQDN
			switch (current_acl_item->matcher)
			{
				case DB_ACL_MATCHER_STRICT:
					if (fqdn_hash == current_list_item->s_fqdn_hash &&
							likely(strncmp(_request_data->fqdn, current_list_item->s_fqdn, current_list_item->s_fqdn_length) == 0))
					{
						matcher_matched = 1;
						goto found;
					}
					break;
				case DB_ACL_MATCHER_SUBDOMAIN:
				{
					char* pos = strstr(_request_data->fqdn, current_list_item->s_fqdn);
					size_t fqdn_len = strlen(_request_data->fqdn);
					size_t list_item_len = strlen(current_list_item->s_fqdn);
					if (pos && (size_t)(pos - _request_data->fqdn) == fqdn_len - list_item_len)
					{
						matcher_matched = 1;
						goto found;
					}
					break;
				}
				case DB_ACL_MATCHER_REGEX:
					if (regexec(&current_list_item->regex, _request_data->fqdn, 0, NULL, 0) == REG_NOERROR)
					{
						matcher_matched = 1;
						goto found;
					}
					break;
				default:
					panic("Unknown matcher");
					break;
			}
		}
found:
		if (!matcher_matched)
			continue;

		// Set action
		ret = current_acl_item->action;
		switch (current_acl_item->action)
		{
			case DB_ACL_ACTION_SET_A:
				*_acl_data_length = sizeof(current_acl_item->action_parameters.set_a);
				*_acl_data = pfcq_alloc(*_acl_data_length);
				memcpy(*_acl_data, &current_acl_item->action_parameters.set_a, *_acl_data_length);
				break;
			default:
				break;
		}
		if (unlikely(pthread_spin_lock(&current_acl_item->hits_lock)))
			panic("pthread_spin_lock");
		current_acl_item->hits++;
		if (unlikely(pthread_spin_unlock(&current_acl_item->hits_lock)))
			panic("pthread_spin_unlock");
		break;
	}

	return ret;
}

