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

#include "types.h"
#include "utils.h"

#include "stats.h"

void db_stats_frontend_in(struct db_frontend* _frontend, uint64_t _delta_bytes)
{
	if (unlikely(pthread_spin_lock(&_frontend->stats.in_lock)))
		panic("pthread_spin_lock");
	_frontend->stats.in_pkts++;
	_frontend->stats.in_bytes += _delta_bytes;
	if (unlikely(pthread_spin_unlock(&_frontend->stats.in_lock)))
		panic("pthread_spin_unlock");

	return;
}

void db_stats_frontend_in_invalid(struct db_frontend* _frontend, uint64_t _delta_bytes)
{
	if (unlikely(pthread_spin_lock(&_frontend->stats.in_invalid_lock)))
		panic("pthread_spin_lock");
	_frontend->stats.in_pkts_invalid++;
	_frontend->stats.in_bytes_invalid += _delta_bytes;
	if (unlikely(pthread_spin_unlock(&_frontend->stats.in_invalid_lock)))
		panic("pthread_spin_unlock");

	return;
}

void db_stats_frontend_out(struct db_frontend* _frontend, uint64_t _delta_bytes, ldns_pkt_rcode _rcode)
{
	if (unlikely(pthread_spin_lock(&_frontend->stats.out_lock)))
		panic("pthread_spin_lock");
	_frontend->stats.out_pkts++;
	_frontend->stats.out_bytes += _delta_bytes;
	switch (_rcode)
	{
		case LDNS_RCODE_NOERROR:
			_frontend->stats.out_noerror++;
			break;
		case LDNS_RCODE_SERVFAIL:
			_frontend->stats.out_servfail++;
			break;
		case LDNS_RCODE_NXDOMAIN:
			_frontend->stats.out_nxdomain++;
			break;
		case LDNS_RCODE_REFUSED:
			_frontend->stats.out_refused++;
			break;
		default:
			_frontend->stats.out_other++;
			break;
	}
	if (unlikely(pthread_spin_unlock(&_frontend->stats.out_lock)))
		panic("pthread_spin_unlock");

	return;
}

static struct db_frontend_stats db_stats_frontend(struct db_frontend* _frontend)
{
	if (unlikely(pthread_spin_lock(&_frontend->stats.in_lock)))
		panic("pthread_spin_lock");
	if (unlikely(pthread_spin_lock(&_frontend->stats.in_invalid_lock)))
		panic("pthread_spin_lock");
	if (unlikely(pthread_spin_lock(&_frontend->stats.out_lock)))
		panic("pthread_spin_lock");

	struct db_frontend_stats ret = _frontend->stats;

	if (unlikely(pthread_spin_unlock(&_frontend->stats.out_lock)))
		panic("pthread_spin_unlock");
	if (unlikely(pthread_spin_unlock(&_frontend->stats.in_invalid_lock)))
		panic("pthread_spin_unlock");
	if (unlikely(pthread_spin_unlock(&_frontend->stats.in_lock)))
		panic("pthread_spin_unlock");

	return ret;
}

void db_stats_forwarder_in(struct db_forwarder* _forwarder, uint64_t _delta_bytes)
{
	if (unlikely(pthread_spin_lock(&_forwarder->stats.in_lock)))
		panic("pthread_spin_lock");
	_forwarder->stats.in_pkts++;
	_forwarder->stats.in_bytes += _delta_bytes;
	if (unlikely(pthread_spin_unlock(&_forwarder->stats.in_lock)))
		panic("pthread_spin_unlock");

	return;
}

void db_stats_forwarder_out(struct db_forwarder* _forwarder, uint64_t _delta_bytes, ldns_pkt_rcode _rcode)
{
	if (unlikely(pthread_spin_lock(&_forwarder->stats.out_lock)))
		panic("pthread_spin_lock");
	_forwarder->stats.out_pkts++;
	_forwarder->stats.out_bytes += _delta_bytes;
	switch (_rcode)
	{
		case LDNS_RCODE_NOERROR:
			_forwarder->stats.out_noerror++;
			break;
		case LDNS_RCODE_SERVFAIL:
			_forwarder->stats.out_servfail++;
			break;
		case LDNS_RCODE_NXDOMAIN:
			_forwarder->stats.out_nxdomain++;
			break;
		case LDNS_RCODE_REFUSED:
			_forwarder->stats.out_refused++;
			break;
		default:
			_forwarder->stats.out_other++;
			break;
	}
	if (unlikely(pthread_spin_unlock(&_forwarder->stats.out_lock)))
		panic("pthread_spin_unlock");

	return;
}

static struct db_forwarder_stats db_stats_forwarder(struct db_forwarder* _forwarder)
{
	if (unlikely(pthread_spin_lock(&_forwarder->stats.in_lock)))
		panic("pthread_spin_lock");
	if (unlikely(pthread_spin_lock(&_forwarder->stats.out_lock)))
		panic("pthread_spin_lock");

	struct db_forwarder_stats ret = _forwarder->stats;

	if (unlikely(pthread_spin_unlock(&_forwarder->stats.out_lock)))
		panic("pthread_spin_unlock");
	if (unlikely(pthread_spin_unlock(&_forwarder->stats.in_lock)))
		panic("pthread_spin_unlock");

	return ret;
}

static int db_queue_code(struct MHD_Connection* _connection, const char* _url, unsigned int _code)
{
	int ret = MHD_NO;
	struct MHD_Response* response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
	if (unlikely(!response))
		return ret;

	ret = MHD_queue_response(_connection, _code, response);
	MHD_destroy_response(response);

	return ret;
}

static int db_answer_to_connection(void* _data,
		struct MHD_Connection* _connection,
		const char* _url, const char* _method,
		const char* _version, const char* _upload_data,
		size_t* _upload_data_size, void** _context)
{
	(void)_version;
	(void)_upload_data;
	(void)_upload_data_size;
	(void)_context;

	int ret = MHD_NO;
	struct MHD_Response* response = NULL;
	char* body = NULL;
	struct db_local_context* l_ctx = (struct db_local_context*)_data;

	if (unlikely(strcmp(_method, MHD_HTTP_METHOD_GET) != 0))
	{
		ret = db_queue_code(_connection, _url, MHD_HTTP_METHOD_NOT_ALLOWED);

		goto error;
	}

	if (strcmp(_url, "/stats") == 0)
	{
		body = pfcq_mstring("%s\n", "# name,FRONTEND,in_pkts,in_bytes,in_invalid_pkts,in_invalid_bytes,out_pkts,out_bytes,noerror,servfail,nxdomain,refused,other");
		for (size_t i = 0; i < l_ctx->frontends_count; i++)
		{
			struct db_frontend_stats fe_stats = db_stats_frontend(l_ctx->frontends[i]);
			char* row = pfcq_mstring("%s,FRONTEND,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
					l_ctx->frontends[i]->name,
					fe_stats.in_pkts, fe_stats.in_bytes,
					fe_stats.in_pkts_invalid, fe_stats.in_bytes_invalid,
					fe_stats.out_pkts, fe_stats.out_bytes,
					fe_stats.out_noerror, fe_stats.out_servfail, fe_stats.out_nxdomain, fe_stats.out_refused, fe_stats.out_other);
			body = pfcq_cstring(body, row);
			pfcq_free(row);
		}
		body = pfcq_cstring(body, "# name,FORWARDER,frontend_name,in_pkts,in_bytes,out_pkts,out_bytes,noerror,servfail,nxdomain,refused,other\n");
		for (size_t i = 0; i < l_ctx->frontends_count; i++)
			for (size_t j = 0; j < l_ctx->frontends[i]->backend.forwarders_count; j++)
			{
				struct db_forwarder_stats frw_stats = db_stats_forwarder(l_ctx->frontends[i]->backend.forwarders[j]);
				char* row = pfcq_mstring("%s,FORWARDER,%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
						l_ctx->frontends[i]->backend.forwarders[j]->name,
						l_ctx->frontends[i]->name,
						frw_stats.in_pkts, frw_stats.in_bytes,
						frw_stats.out_pkts, frw_stats.out_bytes,
						frw_stats.out_noerror, frw_stats.out_servfail, frw_stats.out_nxdomain, frw_stats.out_refused, frw_stats.out_other);
				body = pfcq_cstring(body, row);
				pfcq_free(row);
			}

		goto noerror;
	} else if (strcmp(_url, "/acls") == 0)
	{
		body = pfcq_mstring("%s\n", "# ACLs");
		for (size_t i = 0; i < l_ctx->frontends_count; i++)
		{
			char* row = NULL;
			struct db_acl_item* current_acl_item = NULL;

			row = pfcq_mstring("# %s,FRONTEND\n", l_ctx->frontends[i]->name);
			body = pfcq_cstring(body, row);
			pfcq_free(row);
			row = pfcq_mstring("%s\n", "# layer3,address/netmask,regex,action,hits");
			body = pfcq_cstring(body, row);
			pfcq_free(row);

			TAILQ_FOREACH(current_acl_item, &l_ctx->frontends[i]->acl, tailq)
			{
				if (unlikely(pthread_spin_lock(&current_acl_item->hits_lock)))
					panic("pthread_spin_lock");
				uint64_t hits = current_acl_item->hits;
				if (unlikely(pthread_spin_unlock(&current_acl_item->hits_lock)))
					panic("pthread_spin_unlock");
				row = pfcq_mstring("%s,%s/%s,%s,%s,%lu\n", current_acl_item->s_layer3,
						current_acl_item->s_address, current_acl_item->s_netmask,
						current_acl_item->s_list, current_acl_item->s_action,
						hits);
				body = pfcq_cstring(body, row);
				pfcq_free(row);
			}
		}

		goto noerror;
	}  else if (strcmp(_url, "/lats") == 0)
	{
		body = pfcq_mstring("%s\n", "# us, hits");
		for (size_t i = 0; i < DB_LATENCY_BUCKETS - 1; i++)
		{
			char* row = NULL;
			if (unlikely(pthread_spin_lock(&l_ctx->db_lats.lats_lock[i])))
				panic("pthread_spin_lock");
			row = pfcq_mstring("LAT,%lu,%lu\n", 1UL << i, l_ctx->db_lats.lats[i]);
			if (unlikely(pthread_spin_unlock(&l_ctx->db_lats.lats_lock[i])))
				panic("pthread_spin_unlock");
			body = pfcq_cstring(body, row);
			pfcq_free(row);
		}
		if (unlikely(pthread_spin_lock(&l_ctx->db_lats.lats_lock[DB_LATENCY_BUCKETS - 1])))
			panic("pthread_spin_lock");
		char* max_row = pfcq_mstring("LAT,MAX,%lu\n", l_ctx->db_lats.lats[DB_LATENCY_BUCKETS - 1]);
		if (unlikely(pthread_spin_unlock(&l_ctx->db_lats.lats_lock[DB_LATENCY_BUCKETS - 1])))
			panic("pthread_spin_unlock");
		body = pfcq_cstring(body, max_row);
		pfcq_free(max_row);

		goto noerror;
	} else if (strcmp(_url, "/queue") == 0)
	{
		size_t requests_count = 0;
		if (unlikely(pthread_spin_lock(&l_ctx->global_context->db_requests.requests_count_lock)))
			panic("pthread_spin_lock");
		requests_count = l_ctx->global_context->db_requests.requests_count;
		if (unlikely(pthread_spin_unlock(&l_ctx->global_context->db_requests.requests_count_lock)))
			panic("pthread_spin_unlock");
		body = pfcq_mstring("%lu", requests_count);

		goto noerror;
	} else if (strcmp(_url, "/ping") == 0)
	{
		body = pfcq_strdup("OK");

		goto noerror;
	} else
	{
		ret = db_queue_code(_connection, _url, MHD_HTTP_NOT_FOUND);

		goto error;
	}

noerror:
	response = MHD_create_response_from_buffer(strlen(body), body, MHD_RESPMEM_MUST_COPY);
	if (unlikely(!response))
	{
		ret = db_queue_code(_connection, _url, MHD_HTTP_INTERNAL_SERVER_ERROR);
	} else
	{
		ret = MHD_queue_response(_connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
	}
	pfcq_free(body);

error:
	return ret;
}

void db_stats_latency_update(struct db_local_context* _ctx, struct timespec _ctime)
{
	struct timespec time_now;
	unsigned bucket = 0;

	if (unlikely(clock_gettime(CLOCK_REALTIME, &time_now)))
		panic("clock_gettime");

	bucket = DB_LOG2(__pfcq_timespec_diff_ns(_ctime, time_now) / 1000);
	if (bucket >= DB_LATENCY_BUCKETS)
		bucket = DB_LATENCY_BUCKETS - 1;

	if (unlikely(pthread_spin_lock(&_ctx->db_lats.lats_lock[bucket])))
		panic("pthread_spin_lock");
	_ctx->db_lats.lats[bucket]++;
	if (unlikely(pthread_spin_unlock(&_ctx->db_lats.lats_lock[bucket])))
		panic("pthread_spin_unlock");

	return;
}

void db_stats_init(struct db_local_context* _ctx)
{
	pfcq_zero(&_ctx->db_lats, sizeof(struct db_latency_stats));
	for (size_t i = 0; i < DB_LATENCY_BUCKETS; i++)
		if (unlikely(pthread_spin_init(&_ctx->db_lats.lats_lock[i], PTHREAD_PROCESS_PRIVATE)))
			panic("pthread_spin_init");

	if (_ctx->stats_enabled)
	{
		unsigned int options = MHD_USE_SELECT_INTERNALLY | MHD_USE_EPOLL_LINUX_ONLY;
		switch (_ctx->stats_layer3_family)
		{
			case PF_INET:
				_ctx->mhd_daemon =
					MHD_start_daemon(options,
							0, NULL, NULL, &db_answer_to_connection, (void*)_ctx,
							MHD_OPTION_THREAD_POOL_SIZE, 1,
							MHD_OPTION_SOCK_ADDR,
							(struct sockaddr*)&_ctx->stats_address.address4,
							MHD_OPTION_END);
				break;
			case PF_INET6:
				_ctx->mhd_daemon =
					MHD_start_daemon(options | MHD_USE_IPv6,
							0, NULL, NULL, &db_answer_to_connection, (void*)_ctx,
							MHD_OPTION_THREAD_POOL_SIZE, 1,
							MHD_OPTION_SOCK_ADDR,
							(struct sockaddr*)&_ctx->stats_address.address6,
							MHD_OPTION_END);
				break;
			default:
				panic("socket domain");
				break;
		}
		if (unlikely(_ctx->mhd_daemon == NULL))
			panic("MHD_start_daemon");
	}

	return;
}

void db_stats_done(struct db_local_context* _ctx)
{
	if (_ctx->mhd_daemon)
		MHD_stop_daemon(_ctx->mhd_daemon);
	for (size_t i = 0; i < DB_LATENCY_BUCKETS; i++)
		if (unlikely(pthread_spin_destroy(&_ctx->db_lats.lats_lock[i])))
			panic("pthread_spin_destroy");

	return;
}

