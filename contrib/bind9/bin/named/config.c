/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2001-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: config.c,v 1.11.2.4.8.29 2004/10/05 02:52:26 marka Exp $ */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <isc/buffer.h>
#include <isc/log.h>
#include <isc/mem.h>
#include <isc/region.h>
#include <isc/result.h>
#include <isc/sockaddr.h>
#include <isc/util.h>

#include <isccfg/namedconf.h>

#include <dns/fixedname.h>
#include <dns/name.h>
#include <dns/rdataclass.h>
#include <dns/rdatatype.h>
#include <dns/tsig.h>
#include <dns/zone.h>

#include <named/config.h>
#include <named/globals.h>

static char defaultconf[] = "\
options {\n\
#	blackhole {none;};\n"
#ifndef WIN32
"	coresize default;\n\
	datasize default;\n\
	files default;\n\
	stacksize default;\n"
#endif
"	deallocate-on-exit true;\n\
#	directory <none>\n\
	dump-file \"named_dump.db\";\n\
	fake-iquery no;\n\
	has-old-clients false;\n\
	heartbeat-interval 60;\n\
	host-statistics no;\n\
	interface-interval 60;\n\
	listen-on {any;};\n\
	listen-on-v6 {none;};\n\
	match-mapped-addresses no;\n\
	memstatistics-file \"named.memstats\";\n\
	multiple-cnames no;\n\
#	named-xfer <obsolete>;\n\
#	pid-file \"" NS_LOCALSTATEDIR "/named.pid\"; /* or /lwresd.pid */\n\
	port 53;\n\
	recursing-file \"named.recursing\";\n\
"
#ifdef PATH_RANDOMDEV
"\
	random-device \"" PATH_RANDOMDEV "\";\n\
"
#endif
"\
	recursive-clients 1000;\n\
	rrset-order {order cyclic;};\n\
	serial-queries 20;\n\
	serial-query-rate 20;\n\
	server-id none;\n\
	statistics-file \"named.stats\";\n\
	statistics-interval 60;\n\
	tcp-clients 100;\n\
	tcp-listen-queue 3;\n\
#	tkey-dhkey <none>\n\
#	tkey-gssapi-credential <none>\n\
#	tkey-domain <none>\n\
	transfers-per-ns 2;\n\
	transfers-in 10;\n\
	transfers-out 10;\n\
	treat-cr-as-space true;\n\
	use-id-pool true;\n\
	use-ixfr true;\n\
	edns-udp-size 4096;\n\
\n\
	/* view */\n\
	allow-notify {none;};\n\
	allow-update-forwarding {none;};\n\
	allow-recursion {any;};\n\
#	allow-v6-synthesis <obsolete>;\n\
#	sortlist <none>\n\
#	topology <none>\n\
	auth-nxdomain false;\n\
	minimal-responses false;\n\
	recursion true;\n\
	provide-ixfr true;\n\
	request-ixfr true;\n\
	fetch-glue no;\n\
	rfc2308-type1 no;\n\
	additional-from-auth true;\n\
	additional-from-cache true;\n\
	query-source address *;\n\
	query-source-v6 address *;\n\
	notify-source *;\n\
	notify-source-v6 *;\n\
	cleaning-interval 60;\n\
	min-roots 2;\n\
	lame-ttl 600;\n\
	max-ncache-ttl 10800; /* 3 hours */\n\
	max-cache-ttl 604800; /* 1 week */\n\
	transfer-format many-answers;\n\
	max-cache-size 0;\n\
	check-names master fail;\n\
	check-names slave warn;\n\
	check-names response ignore;\n\
	dnssec-enable no; /* Make yes for 9.4. */ \n\
"

"	/* zone */\n\
	allow-query {any;};\n\
	allow-transfer {any;};\n\
	notify yes;\n\
#	also-notify <none>\n\
	dialup no;\n\
#	forward <none>\n\
#	forwarders <none>\n\
	maintain-ixfr-base no;\n\
#	max-ixfr-log-size <obsolete>\n\
	transfer-source *;\n\
	transfer-source-v6 *;\n\
	alt-transfer-source *;\n\
	alt-transfer-source-v6 *;\n\
	max-transfer-time-in 120;\n\
	max-transfer-time-out 120;\n\
	max-transfer-idle-in 60;\n\
	max-transfer-idle-out 60;\n\
	max-retry-time 1209600; /* 2 weeks */\n\
	min-retry-time 500;\n\
	max-refresh-time 2419200; /* 4 weeks */\n\
	min-refresh-time 300;\n\
	multi-master no;\n\
	sig-validity-interval 30; /* days */\n\
	zone-statistics false;\n\
	max-journal-size unlimited;\n\
	ixfr-from-differences false;\n\
};\n\
"

"#\n\
#  Zones in the \"_bind\" view are NOT counted is the count of zones.\n\
#\n\
view \"_bind\" chaos {\n\
	recursion no;\n\
	notify no;\n\
\n\
	zone \"version.bind\" chaos {\n\
		type master;\n\
		database \"_builtin version\";\n\
	};\n\
\n\
	zone \"hostname.bind\" chaos {\n\
		type master;\n\
		database \"_builtin hostname\";\n\
	};\n\
\n\
	zone \"authors.bind\" chaos {\n\
		type master;\n\
		database \"_builtin authors\";\n\
	};\n\
	zone \"id.server\" chaos {\n\
		type master;\n\
		database \"_builtin id\";\n\
	};\n\
};\n\
";

isc_result_t
ns_config_parsedefaults(cfg_parser_t *parser, cfg_obj_t **conf) {
	isc_buffer_t b;

	isc_buffer_init(&b, defaultconf, sizeof(defaultconf) - 1);
	isc_buffer_add(&b, sizeof(defaultconf) - 1);
	return (cfg_parse_buffer(parser, &b, &cfg_type_namedconf, conf));
}

isc_result_t
ns_config_get(cfg_obj_t **maps, const char *name, cfg_obj_t **obj) {
	int i;

	for (i = 0;; i++) {
		if (maps[i] == NULL)
			return (ISC_R_NOTFOUND);
		if (cfg_map_get(maps[i], name, obj) == ISC_R_SUCCESS)
			return (ISC_R_SUCCESS);
	}
}

isc_result_t
ns_checknames_get(cfg_obj_t **maps, const char *which, cfg_obj_t **obj) {
	cfg_listelt_t *element;
	cfg_obj_t *checknames;
	cfg_obj_t *type;
	cfg_obj_t *value;
	int i;

	for (i = 0;; i++) {
		if (maps[i] == NULL)
			return (ISC_R_NOTFOUND);
		checknames = NULL;
		if (cfg_map_get(maps[i], "check-names", &checknames) == ISC_R_SUCCESS) {
			/*
			 * Zone map entry is not a list.
			 */
			if (checknames != NULL && !cfg_obj_islist(checknames)) {
				*obj = checknames;
				return (ISC_R_SUCCESS);
			}
			for (element = cfg_list_first(checknames);
			     element != NULL;
			     element = cfg_list_next(element)) {
				value = cfg_listelt_value(element);
				type = cfg_tuple_get(value, "type");
				if (strcasecmp(cfg_obj_asstring(type), which) == 0) {
					*obj = cfg_tuple_get(value, "mode");
					return (ISC_R_SUCCESS);
				}
			}

		}
	}
}

int
ns_config_listcount(cfg_obj_t *list) {
	cfg_listelt_t *e;
	int i = 0;

	for (e = cfg_list_first(list); e != NULL; e = cfg_list_next(e))
		i++;

	return (i);
}

isc_result_t
ns_config_getclass(cfg_obj_t *classobj, dns_rdataclass_t defclass,
		   dns_rdataclass_t *classp) {
	char *str;
	isc_textregion_t r;
	isc_result_t result;

	if (!cfg_obj_isstring(classobj)) {
		*classp = defclass;
		return (ISC_R_SUCCESS);
	}
	str = cfg_obj_asstring(classobj);
	r.base = str;
	r.length = strlen(str);
	result = dns_rdataclass_fromtext(classp, &r);
	if (result != ISC_R_SUCCESS)
		cfg_obj_log(classobj, ns_g_lctx, ISC_LOG_ERROR,
			    "unknown class '%s'", str);
	return (result);
}

isc_result_t
ns_config_gettype(cfg_obj_t *typeobj, dns_rdatatype_t deftype,
		   dns_rdatatype_t *typep) {
	char *str;
	isc_textregion_t r;
	isc_result_t result;

	if (!cfg_obj_isstring(typeobj)) {
		*typep = deftype;
		return (ISC_R_SUCCESS);
	}
	str = cfg_obj_asstring(typeobj);
	r.base = str;
	r.length = strlen(str);
	result = dns_rdatatype_fromtext(typep, &r);
	if (result != ISC_R_SUCCESS)
		cfg_obj_log(typeobj, ns_g_lctx, ISC_LOG_ERROR,
			    "unknown type '%s'", str);
	return (result);
}

dns_zonetype_t
ns_config_getzonetype(cfg_obj_t *zonetypeobj) {
	dns_zonetype_t ztype = dns_zone_none;
	char *str;

	str = cfg_obj_asstring(zonetypeobj);
	if (strcasecmp(str, "master") == 0)
		ztype = dns_zone_master;
	else if (strcasecmp(str, "slave") == 0)
		ztype = dns_zone_slave;
	else if (strcasecmp(str, "stub") == 0)
		ztype = dns_zone_stub;
	else
		INSIST(0);
	return (ztype);
}

isc_result_t
ns_config_getiplist(cfg_obj_t *config, cfg_obj_t *list,
		    in_port_t defport, isc_mem_t *mctx,
		    isc_sockaddr_t **addrsp, isc_uint32_t *countp)
{
	int count, i = 0;
	cfg_obj_t *addrlist;
	cfg_obj_t *portobj;
	cfg_listelt_t *element;
	isc_sockaddr_t *addrs;
	in_port_t port;
	isc_result_t result;

	INSIST(addrsp != NULL && *addrsp == NULL);
	INSIST(countp != NULL);

	addrlist = cfg_tuple_get(list, "addresses");
	count = ns_config_listcount(addrlist);

	portobj = cfg_tuple_get(list, "port");
	if (cfg_obj_isuint32(portobj)) {
		isc_uint32_t val = cfg_obj_asuint32(portobj);
		if (val > ISC_UINT16_MAX) {
			cfg_obj_log(portobj, ns_g_lctx, ISC_LOG_ERROR,
				    "port '%u' out of range", val);
			return (ISC_R_RANGE);
		}
		port = (in_port_t) val;
	} else if (defport != 0)
		port = defport;
	else {
		result = ns_config_getport(config, &port);
		if (result != ISC_R_SUCCESS)
			return (result);
	}

	addrs = isc_mem_get(mctx, count * sizeof(isc_sockaddr_t));
	if (addrs == NULL)
		return (ISC_R_NOMEMORY);

	for (element = cfg_list_first(addrlist);
	     element != NULL;
	     element = cfg_list_next(element), i++)
	{
		INSIST(i < count);
		addrs[i] = *cfg_obj_assockaddr(cfg_listelt_value(element));
		if (isc_sockaddr_getport(&addrs[i]) == 0)
			isc_sockaddr_setport(&addrs[i], port);
	}
	INSIST(i == count);

	*addrsp = addrs;
	*countp = count;

	return (ISC_R_SUCCESS);
}

void
ns_config_putiplist(isc_mem_t *mctx, isc_sockaddr_t **addrsp,
		    isc_uint32_t count)
{
	INSIST(addrsp != NULL && *addrsp != NULL);

	isc_mem_put(mctx, *addrsp, count * sizeof(isc_sockaddr_t));
	*addrsp = NULL;
}

static isc_result_t
get_masters_def(cfg_obj_t *cctx, char *name, cfg_obj_t **ret) {
	isc_result_t result;
	cfg_obj_t *masters = NULL;
	cfg_listelt_t *elt;

	result = cfg_map_get(cctx, "masters", &masters);
	if (result != ISC_R_SUCCESS)
		return (result);
	for (elt = cfg_list_first(masters);
	     elt != NULL;
	     elt = cfg_list_next(elt)) {
		cfg_obj_t *list;
		const char *listname;

		list = cfg_listelt_value(elt);
		listname = cfg_obj_asstring(cfg_tuple_get(list, "name"));

		if (strcasecmp(listname, name) == 0) {
			*ret = list;
			return (ISC_R_SUCCESS);
		}
	}
	return (ISC_R_NOTFOUND);
}

isc_result_t
ns_config_getipandkeylist(cfg_obj_t *config, cfg_obj_t *list, isc_mem_t *mctx,
			  isc_sockaddr_t **addrsp, dns_name_t ***keysp,
			  isc_uint32_t *countp)
{
	isc_uint32_t addrcount = 0, keycount = 0, i = 0;
	isc_uint32_t listcount = 0, l = 0, j;
	isc_uint32_t stackcount = 0, pushed = 0;
	isc_result_t result;
	cfg_listelt_t *element;
	cfg_obj_t *addrlist;
	cfg_obj_t *portobj;
	in_port_t port;
	dns_fixedname_t fname;
	isc_sockaddr_t *addrs = NULL;
	dns_name_t **keys = NULL;
	char **lists = NULL;
	struct {
		cfg_listelt_t *element;
		in_port_t port;
	} *stack = NULL;

	REQUIRE(addrsp != NULL && *addrsp == NULL);
	REQUIRE(keysp != NULL && *keysp == NULL);
	REQUIRE(countp != NULL);

 newlist:
	addrlist = cfg_tuple_get(list, "addresses");
	portobj = cfg_tuple_get(list, "port");
	if (cfg_obj_isuint32(portobj)) {
		isc_uint32_t val = cfg_obj_asuint32(portobj);
		if (val > ISC_UINT16_MAX) {
			cfg_obj_log(portobj, ns_g_lctx, ISC_LOG_ERROR,
				    "port '%u' out of range", val);
			return (ISC_R_RANGE);
		}
		port = (in_port_t) val;
	} else {
		result = ns_config_getport(config, &port);
		if (result != ISC_R_SUCCESS)
			return (result);
	}

	result = ISC_R_NOMEMORY;

	element = cfg_list_first(addrlist);
 resume:
	for ( ;
	     element != NULL;
	     element = cfg_list_next(element))
	{
		cfg_obj_t *addr;
		cfg_obj_t *key;
		char *keystr;
		isc_buffer_t b;

		addr = cfg_tuple_get(cfg_listelt_value(element),
				     "masterselement");
		key = cfg_tuple_get(cfg_listelt_value(element), "key");

		if (!cfg_obj_issockaddr(addr)) {
			char *listname = cfg_obj_asstring(addr);
			isc_result_t tresult;

			/* Grow lists? */
			if (listcount == l) {
				void * new;
				isc_uint32_t newlen = listcount + 16;
				size_t newsize, oldsize;

				newsize = newlen * sizeof(*lists);
				oldsize = listcount * sizeof(*lists);
				new = isc_mem_get(mctx, newsize);
				if (new == NULL)
					goto cleanup;
				if (listcount != 0) {
					memcpy(new, lists, oldsize);
					isc_mem_put(mctx, lists, oldsize);
				}
				lists = new;
				listcount = newlen;
			}
			/* Seen? */
			for (j = 0; j < l; j++)
				if (strcasecmp(lists[j], listname) == 0)
					break;
			if (j < l)
				continue;
			tresult = get_masters_def(config, listname, &list);
			if (tresult == ISC_R_NOTFOUND) {
				cfg_obj_log(addr, ns_g_lctx, ISC_LOG_ERROR,
                                    "masters \"%s\" not found", listname);

				result = tresult;
				goto cleanup;
			}
			if (tresult != ISC_R_SUCCESS)
				goto cleanup;
			lists[l++] = listname;
			/* Grow stack? */
			if (stackcount == pushed) {
				void * new;
				isc_uint32_t newlen = stackcount + 16;
				size_t newsize, oldsize;

				newsize = newlen * sizeof(*stack);
				oldsize = stackcount * sizeof(*stack);
				new = isc_mem_get(mctx, newsize);
				if (new == NULL)
					goto cleanup;
				if (stackcount != 0) {
					memcpy(new, stack, oldsize);
					isc_mem_put(mctx, stack, oldsize);
				}
				stack = new;
				stackcount = newlen;
			}
			/*
			 * We want to resume processing this list on the
			 * next element.
			 */
			stack[pushed].element = cfg_list_next(element);
			stack[pushed].port = port;
			pushed++;
			goto newlist;
		}

		if (i == addrcount) {
			void * new;
			isc_uint32_t newlen = addrcount + 16;
			size_t newsize, oldsize;

			newsize = newlen * sizeof(isc_sockaddr_t);
			oldsize = addrcount * sizeof(isc_sockaddr_t);
			new = isc_mem_get(mctx, newsize);
			if (new == NULL)
				goto cleanup;
			if (addrcount != 0) {
				memcpy(new, addrs, oldsize);
				isc_mem_put(mctx, addrs, oldsize);
			}
			addrs = new;
			addrcount = newlen;

			newsize = newlen * sizeof(dns_name_t *);
			oldsize = keycount * sizeof(dns_name_t *);
			new = isc_mem_get(mctx, newsize);
			if (new == NULL)
				goto cleanup;
			if (keycount != 0) {
				memcpy(new, keys, oldsize);
				isc_mem_put(mctx, keys, oldsize);
			}
			keys = new;
			keycount = newlen;
		}

		addrs[i] = *cfg_obj_assockaddr(addr);
		if (isc_sockaddr_getport(&addrs[i]) == 0)
			isc_sockaddr_setport(&addrs[i], port);
		keys[i] = NULL;
		if (!cfg_obj_isstring(key)) {
			i++;
			continue;
		}
		keys[i] = isc_mem_get(mctx, sizeof(dns_name_t));
		if (keys[i] == NULL)
			goto cleanup;
		dns_name_init(keys[i], NULL);
		
		keystr = cfg_obj_asstring(key);
		isc_buffer_init(&b, keystr, strlen(keystr));
		isc_buffer_add(&b, strlen(keystr));
		dns_fixedname_init(&fname);
		result = dns_name_fromtext(dns_fixedname_name(&fname), &b,
					   dns_rootname, ISC_FALSE, NULL);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		result = dns_name_dup(dns_fixedname_name(&fname), mctx,
				      keys[i]);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		i++;
	}
	if (pushed != 0) {
		pushed--;
		element = stack[pushed].element;
		port = stack[pushed].port;
		goto resume;
	}
	if (i < addrcount) {
		void * new;
		size_t newsize, oldsize;

		newsize = i * sizeof(isc_sockaddr_t);
		oldsize = addrcount * sizeof(isc_sockaddr_t);
		if (i != 0) {
			new = isc_mem_get(mctx, newsize);
			if (new == NULL)
				goto cleanup;
			memcpy(new, addrs, newsize);
			isc_mem_put(mctx, addrs, oldsize);
		} else
			new = NULL;
		addrs = new;
		addrcount = i;

		newsize = i * sizeof(dns_name_t *);
		oldsize = keycount * sizeof(dns_name_t *);
		if (i != 0) {
			new = isc_mem_get(mctx, newsize);
			if (new == NULL)
				goto cleanup;
			memcpy(new, keys,  newsize);
			isc_mem_put(mctx, keys, oldsize);
		} else
			new = NULL;
		keys = new;
		keycount = i;
	}

	if (lists != NULL)
		isc_mem_put(mctx, lists, listcount * sizeof(*lists));
	if (stack != NULL)
		isc_mem_put(mctx, stack, stackcount * sizeof(*stack));
	
	INSIST(keycount == addrcount);

	*addrsp = addrs;
	*keysp = keys;
	*countp = addrcount;

	return (ISC_R_SUCCESS);

 cleanup:
	if (addrs != NULL)
		isc_mem_put(mctx, addrs, addrcount * sizeof(isc_sockaddr_t));
	if (keys != NULL) {
		for (j = 0; j <= i; j++) {
			if (keys[j] == NULL)
				continue;
			if (dns_name_dynamic(keys[j]))
				dns_name_free(keys[j], mctx);
			isc_mem_put(mctx, keys[j], sizeof(dns_name_t));
		}
		isc_mem_put(mctx, keys, keycount * sizeof(dns_name_t *));
	}
	if (lists != NULL)
		isc_mem_put(mctx, lists, listcount * sizeof(*lists));
	if (stack != NULL)
		isc_mem_put(mctx, stack, stackcount * sizeof(*stack));
	return (result);
}

void
ns_config_putipandkeylist(isc_mem_t *mctx, isc_sockaddr_t **addrsp,
			  dns_name_t ***keysp, isc_uint32_t count)
{
	unsigned int i;
	dns_name_t **keys = *keysp;

	INSIST(addrsp != NULL && *addrsp != NULL);

	isc_mem_put(mctx, *addrsp, count * sizeof(isc_sockaddr_t));
	for (i = 0; i < count; i++) {
		if (keys[i] == NULL)
			continue;
		if (dns_name_dynamic(keys[i]))
			dns_name_free(keys[i], mctx);
		isc_mem_put(mctx, keys[i], sizeof(dns_name_t));
	}
	isc_mem_put(mctx, *keysp, count * sizeof(dns_name_t *));
	*addrsp = NULL;
	*keysp = NULL;
}

isc_result_t
ns_config_getport(cfg_obj_t *config, in_port_t *portp) {
	cfg_obj_t *maps[3];
	cfg_obj_t *options = NULL;
	cfg_obj_t *portobj = NULL;
	isc_result_t result;
	int i;

	(void)cfg_map_get(config, "options", &options);
	i = 0;
	if (options != NULL)
		maps[i++] = options;
	maps[i++] = ns_g_defaults;
	maps[i] = NULL;

	result = ns_config_get(maps, "port", &portobj);
	INSIST(result == ISC_R_SUCCESS);
	if (cfg_obj_asuint32(portobj) >= ISC_UINT16_MAX) {
		cfg_obj_log(portobj, ns_g_lctx, ISC_LOG_ERROR,
			    "port '%u' out of range",
			    cfg_obj_asuint32(portobj));
		return (ISC_R_RANGE);
	}
	*portp = (in_port_t)cfg_obj_asuint32(portobj);
	return (ISC_R_SUCCESS);
}

isc_result_t
ns_config_getkeyalgorithm(const char *str, dns_name_t **name)
{
	if (strcasecmp(str, "hmac-md5") == 0 ||
	    strcasecmp(str, "hmac-md5.sig-alg.reg.int") == 0 ||
	    strcasecmp(str, "hmac-md5.sig-alg.reg.int.") == 0)
	{
		if (name != NULL)
			*name = dns_tsig_hmacmd5_name;
		return (ISC_R_SUCCESS);
	}
	return (ISC_R_NOTFOUND);
}
