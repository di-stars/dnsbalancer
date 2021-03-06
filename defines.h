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

#pragma once

#ifndef __DEFINES_H__
#define __DEFINES_H__

#define DB_HASH_SEED						(0xda9d9374347ffd15)

#define DB_CONFIG_REQUEST_TTL_KEY			"general:request_ttl"
#define DB_CONFIG_GC_INTERVAL_KEY			"general:gc_interval"
#define DB_CONFIG_WATCHDOG_INTERVAL_KEY		"general:watchdog_interval"
#define DB_CONFIG_STATS_ENABLED_KEY			"stats:enabled"
#define DB_CONFIG_STATS_LAYER3_KEY			"stats:layer3"
#define DB_CONFIG_STATS_PORT_KEY			"stats:port"
#define DB_CONFIG_STATS_BIND_KEY			"stats:bind"
#define DB_CONFIG_RLIMIT_KEY				"general:rlimit"
#define DB_CONFIG_FRONTENDS_KEY				"general:frontends"
#define DB_CONFIG_IPV4						"ipv4"
#define DB_CONFIG_IPV6						"ipv6"
#define DB_CONFIG_RR						"rr"
#define DB_CONFIG_RANDOM					"random"
#define DB_CONFIG_LEAST_PKTS				"least_pkts"
#define DB_CONFIG_LEAST_TRAFFIC				"least_traffic"
#define DB_CONFIG_HASH_L3_L4				"hash_l3+l4"
#define DB_CONFIG_HASH_L3					"hash_l3"
#define DB_CONFIG_HASH_L4					"hash_l4"
#define DB_CONFIG_LIST_SEPARATOR			","
#define DB_CONFIG_PARAMETERS_SEPARATOR		"/"
#define DB_CONFIG_ACL_MATCHER_STRICT		"strict"
#define DB_CONFIG_ACL_MATCHER_SUBDOMAIN		"subdomain"
#define DB_CONFIG_ACL_MATCHER_REGEX			"regex"
#define DB_CONFIG_ACL_RR_TYPE_ALL			"all"
#define DB_CONFIG_ACL_RR_TYPE_ANY			"any"
#define DB_CONFIG_ACL_ACTION_ALLOW			"allow"
#define DB_CONFIG_ACL_ACTION_DENY			"deny"
#define DB_CONFIG_ACL_ACTION_NXDOMAIN		"nxdomain"
#define DB_CONFIG_ACL_ACTION_SET_A			"set_a"
#define DB_CONFIG_ACL_SOURCE_LOCAL			"local"
#define DB_CONFIG_ACL_SOURCE_MYSQL			"mysql"
#define DB_CONFIG_RELOAD_RETRY_KEY			"reload_retry"
#define DB_DEFAULT_RLIMIT					32768
#define DB_DEFAULT_REQUEST_TTL				10000
#define DB_DEFAULT_GC_INTERVAL				1000
#define DB_DEFAULT_WATCHDOG_INTERVAL		1000
#define DB_DEFAULT_STATS_PORT				8083
#define DB_DEFAULT_DNS_PORT					53
#define DB_DEFAULT_DNS_PACKET_SIZE			4096
#define DB_DEFAULT_FORWARDER_CHECK_ATTEMPTS	3
#define DB_DEFAULT_FORWARDER_CHECK_TIMEOUT	500
#define DB_DEFAULT_WEIGHT					1
#define DB_LATENCY_BUCKETS					25
#define DB_DEFAULT_RELOAD_RETRY				500

#endif /* __DEFINES_H__ */

