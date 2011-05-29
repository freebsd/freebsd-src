/*
 * Copyright (C) 2004-2011  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

/* $Id: server.c,v 1.520.12.21 2011-01-14 23:45:49 tbox Exp $ */

/*! \file */

#include <config.h>

#include <stdlib.h>
#include <unistd.h>

#include <isc/app.h>
#include <isc/base64.h>
#include <isc/dir.h>
#include <isc/entropy.h>
#include <isc/file.h>
#include <isc/hash.h>
#include <isc/httpd.h>
#include <isc/lex.h>
#include <isc/parseint.h>
#include <isc/portset.h>
#include <isc/print.h>
#include <isc/resource.h>
#include <isc/socket.h>
#include <isc/stats.h>
#include <isc/stdio.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>
#include <isc/xml.h>

#include <isccfg/namedconf.h>

#include <bind9/check.h>

#include <dns/acache.h>
#include <dns/adb.h>
#include <dns/cache.h>
#include <dns/db.h>
#include <dns/dispatch.h>
#ifdef DLZ
#include <dns/dlz.h>
#endif
#include <dns/forward.h>
#include <dns/journal.h>
#include <dns/keytable.h>
#include <dns/lib.h>
#include <dns/master.h>
#include <dns/masterdump.h>
#include <dns/order.h>
#include <dns/peer.h>
#include <dns/portlist.h>
#include <dns/rbt.h>
#include <dns/rdataclass.h>
#include <dns/rdataset.h>
#include <dns/rdatastruct.h>
#include <dns/resolver.h>
#include <dns/rootns.h>
#include <dns/secalg.h>
#include <dns/stats.h>
#include <dns/tkey.h>
#include <dns/tsig.h>
#include <dns/view.h>
#include <dns/zone.h>
#include <dns/zt.h>

#include <dst/dst.h>
#include <dst/result.h>

#include <named/client.h>
#include <named/config.h>
#include <named/control.h>
#include <named/interfacemgr.h>
#include <named/log.h>
#include <named/logconf.h>
#include <named/lwresd.h>
#include <named/main.h>
#include <named/os.h>
#include <named/server.h>
#include <named/statschannel.h>
#include <named/tkeyconf.h>
#include <named/tsigconf.h>
#include <named/zoneconf.h>
#ifdef HAVE_LIBSCF
#include <named/ns_smf_globals.h>
#include <stdlib.h>
#endif

/*%
 * Check an operation for failure.  Assumes that the function
 * using it has a 'result' variable and a 'cleanup' label.
 */
#define CHECK(op) \
	do { result = (op);					 \
	       if (result != ISC_R_SUCCESS) goto cleanup;	 \
	} while (0)

#define CHECKM(op, msg) \
	do { result = (op);					  \
	       if (result != ISC_R_SUCCESS) {			  \
			isc_log_write(ns_g_lctx,		  \
				      NS_LOGCATEGORY_GENERAL,	  \
				      NS_LOGMODULE_SERVER,	  \
				      ISC_LOG_ERROR,		  \
				      "%s: %s", msg,		  \
				      isc_result_totext(result)); \
			goto cleanup;				  \
		}						  \
	} while (0)						  \

#define CHECKMF(op, msg, file) \
	do { result = (op);					  \
	       if (result != ISC_R_SUCCESS) {			  \
			isc_log_write(ns_g_lctx,		  \
				      NS_LOGCATEGORY_GENERAL,	  \
				      NS_LOGMODULE_SERVER,	  \
				      ISC_LOG_ERROR,		  \
				      "%s '%s': %s", msg, file,	  \
				      isc_result_totext(result)); \
			goto cleanup;				  \
		}						  \
	} while (0)						  \

#define CHECKFATAL(op, msg) \
	do { result = (op);					  \
	       if (result != ISC_R_SUCCESS)			  \
			fatal(msg, result);			  \
	} while (0)						  \

struct ns_dispatch {
	isc_sockaddr_t			addr;
	unsigned int			dispatchgen;
	dns_dispatch_t			*dispatch;
	ISC_LINK(struct ns_dispatch)	link;
};

struct dumpcontext {
	isc_mem_t			*mctx;
	isc_boolean_t			dumpcache;
	isc_boolean_t			dumpzones;
	FILE				*fp;
	ISC_LIST(struct viewlistentry)	viewlist;
	struct viewlistentry		*view;
	struct zonelistentry		*zone;
	dns_dumpctx_t			*mdctx;
	dns_db_t			*db;
	dns_db_t			*cache;
	isc_task_t			*task;
	dns_dbversion_t			*version;
};

struct viewlistentry {
	dns_view_t			*view;
	ISC_LINK(struct viewlistentry)	link;
	ISC_LIST(struct zonelistentry)	zonelist;
};

struct zonelistentry {
	dns_zone_t			*zone;
	ISC_LINK(struct zonelistentry)	link;
};

/*
 * These zones should not leak onto the Internet.
 */
static const struct {
	const char	*zone;
	isc_boolean_t	rfc1918;
} empty_zones[] = {
#ifdef notyet
	/* RFC 1918 */
	{ "10.IN-ADDR.ARPA", ISC_TRUE },
	{ "16.172.IN-ADDR.ARPA", ISC_TRUE },
	{ "17.172.IN-ADDR.ARPA", ISC_TRUE },
	{ "18.172.IN-ADDR.ARPA", ISC_TRUE },
	{ "19.172.IN-ADDR.ARPA", ISC_TRUE },
	{ "20.172.IN-ADDR.ARPA", ISC_TRUE },
	{ "21.172.IN-ADDR.ARPA", ISC_TRUE },
	{ "22.172.IN-ADDR.ARPA", ISC_TRUE },
	{ "23.172.IN-ADDR.ARPA", ISC_TRUE },
	{ "24.172.IN-ADDR.ARPA", ISC_TRUE },
	{ "25.172.IN-ADDR.ARPA", ISC_TRUE },
	{ "26.172.IN-ADDR.ARPA", ISC_TRUE },
	{ "27.172.IN-ADDR.ARPA", ISC_TRUE },
	{ "28.172.IN-ADDR.ARPA", ISC_TRUE },
	{ "29.172.IN-ADDR.ARPA", ISC_TRUE },
	{ "30.172.IN-ADDR.ARPA", ISC_TRUE },
	{ "31.172.IN-ADDR.ARPA", ISC_TRUE },
	{ "168.192.IN-ADDR.ARPA", ISC_TRUE },
#endif

	/* RFC 5735 and RFC 5737 */
	{ "0.IN-ADDR.ARPA", ISC_FALSE },	/* THIS NETWORK */
	{ "127.IN-ADDR.ARPA", ISC_FALSE },	/* LOOPBACK */
	{ "254.169.IN-ADDR.ARPA", ISC_FALSE },	/* LINK LOCAL */
	{ "2.0.192.IN-ADDR.ARPA", ISC_FALSE },	/* TEST NET */
	{ "100.51.198.IN-ADDR.ARPA", ISC_FALSE },	/* TEST NET 2 */
	{ "113.0.203.IN-ADDR.ARPA", ISC_FALSE },	/* TEST NET 3 */
	{ "255.255.255.255.IN-ADDR.ARPA", ISC_FALSE },	/* BROADCAST */

	/* Local IPv6 Unicast Addresses */
	{ "0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.IP6.ARPA", ISC_FALSE },
	{ "1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.IP6.ARPA", ISC_FALSE },
	/* LOCALLY ASSIGNED LOCAL ADDRESS SCOPE */
	{ "D.F.IP6.ARPA", ISC_FALSE },
	{ "8.E.F.IP6.ARPA", ISC_FALSE },	/* LINK LOCAL */
	{ "9.E.F.IP6.ARPA", ISC_FALSE },	/* LINK LOCAL */
	{ "A.E.F.IP6.ARPA", ISC_FALSE },	/* LINK LOCAL */
	{ "B.E.F.IP6.ARPA", ISC_FALSE },	/* LINK LOCAL */

	/* Example Prefix, RFC 3849. */
	{ "8.B.D.0.1.0.0.2.IP6.ARPA", ISC_FALSE },

	{ NULL, ISC_FALSE }
};

static void
fatal(const char *msg, isc_result_t result);

static void
ns_server_reload(isc_task_t *task, isc_event_t *event);

static isc_result_t
ns_listenelt_fromconfig(const cfg_obj_t *listener, const cfg_obj_t *config,
			cfg_aclconfctx_t *actx,
			isc_mem_t *mctx, ns_listenelt_t **target);
static isc_result_t
ns_listenlist_fromconfig(const cfg_obj_t *listenlist, const cfg_obj_t *config,
			 cfg_aclconfctx_t *actx,
			 isc_mem_t *mctx, ns_listenlist_t **target);

static isc_result_t
configure_forward(const cfg_obj_t *config, dns_view_t *view, dns_name_t *origin,
		  const cfg_obj_t *forwarders, const cfg_obj_t *forwardtype);

static isc_result_t
configure_alternates(const cfg_obj_t *config, dns_view_t *view,
		     const cfg_obj_t *alternates);

static isc_result_t
configure_zone(const cfg_obj_t *config, const cfg_obj_t *zconfig,
	       const cfg_obj_t *vconfig, isc_mem_t *mctx, dns_view_t *view,
	       cfg_aclconfctx_t *aclconf);

static void
end_reserved_dispatches(ns_server_t *server, isc_boolean_t all);

/*%
 * Configure a single view ACL at '*aclp'.  Get its configuration from
 * 'vconfig' (for per-view configuration) and maybe from 'config'
 */
static isc_result_t
configure_view_acl(const cfg_obj_t *vconfig, const cfg_obj_t *config,
		   const char *aclname, cfg_aclconfctx_t *actx,
		   isc_mem_t *mctx, dns_acl_t **aclp)
{
	isc_result_t result;
	const cfg_obj_t *maps[3];
	const cfg_obj_t *aclobj = NULL;
	int i = 0;

	if (*aclp != NULL)
		dns_acl_detach(aclp);
	if (vconfig != NULL)
		maps[i++] = cfg_tuple_get(vconfig, "options");
	if (config != NULL) {
		const cfg_obj_t *options = NULL;
		(void)cfg_map_get(config, "options", &options);
		if (options != NULL)
			maps[i++] = options;
	}
	maps[i] = NULL;

	(void)ns_config_get(maps, aclname, &aclobj);
	if (aclobj == NULL)
		/*
		 * No value available.	*aclp == NULL.
		 */
		return (ISC_R_SUCCESS);

	result = cfg_acl_fromconfig(aclobj, config, ns_g_lctx,
				    actx, mctx, 0, aclp);

	return (result);
}


/*%
 * Configure a sortlist at '*aclp'.  Essentially the same as
 * configure_view_acl() except it calls cfg_acl_fromconfig with a
 * nest_level value of 2.
 */
static isc_result_t
configure_view_sortlist(const cfg_obj_t *vconfig, const cfg_obj_t *config,
			cfg_aclconfctx_t *actx, isc_mem_t *mctx,
			dns_acl_t **aclp)
{
	isc_result_t result;
	const cfg_obj_t *maps[3];
	const cfg_obj_t *aclobj = NULL;
	int i = 0;

	if (*aclp != NULL)
		dns_acl_detach(aclp);
	if (vconfig != NULL)
		maps[i++] = cfg_tuple_get(vconfig, "options");
	if (config != NULL) {
		const cfg_obj_t *options = NULL;
		(void)cfg_map_get(config, "options", &options);
		if (options != NULL)
			maps[i++] = options;
	}
	maps[i] = NULL;

	(void)ns_config_get(maps, "sortlist", &aclobj);
	if (aclobj == NULL)
		return (ISC_R_SUCCESS);

	/*
	 * Use a nest level of 3 for the "top level" of the sortlist;
	 * this means each entry in the top three levels will be stored
	 * as lists of separate, nested ACLs, rather than merged together
	 * into IP tables as is usually done with ACLs.
	 */
	result = cfg_acl_fromconfig(aclobj, config, ns_g_lctx,
				    actx, mctx, 3, aclp);

	return (result);
}

static isc_result_t
configure_view_dnsseckey(const cfg_obj_t *vconfig, const cfg_obj_t *key,
			 dns_keytable_t *keytable, isc_mem_t *mctx)
{
	dns_rdataclass_t viewclass;
	dns_rdata_dnskey_t keystruct;
	isc_uint32_t flags, proto, alg;
	const char *keystr, *keynamestr;
	unsigned char keydata[4096];
	isc_buffer_t keydatabuf;
	unsigned char rrdata[4096];
	isc_buffer_t rrdatabuf;
	isc_region_t r;
	dns_fixedname_t fkeyname;
	dns_name_t *keyname;
	isc_buffer_t namebuf;
	isc_result_t result;
	dst_key_t *dstkey = NULL;

	flags = cfg_obj_asuint32(cfg_tuple_get(key, "flags"));
	proto = cfg_obj_asuint32(cfg_tuple_get(key, "protocol"));
	alg = cfg_obj_asuint32(cfg_tuple_get(key, "algorithm"));
	keyname = dns_fixedname_name(&fkeyname);
	keynamestr = cfg_obj_asstring(cfg_tuple_get(key, "name"));

	if (vconfig == NULL)
		viewclass = dns_rdataclass_in;
	else {
		const cfg_obj_t *classobj = cfg_tuple_get(vconfig, "class");
		CHECK(ns_config_getclass(classobj, dns_rdataclass_in,
					 &viewclass));
	}
	keystruct.common.rdclass = viewclass;
	keystruct.common.rdtype = dns_rdatatype_dnskey;
	/*
	 * The key data in keystruct is not dynamically allocated.
	 */
	keystruct.mctx = NULL;

	ISC_LINK_INIT(&keystruct.common, link);

	if (flags > 0xffff)
		CHECKM(ISC_R_RANGE, "key flags");
	if (proto > 0xff)
		CHECKM(ISC_R_RANGE, "key protocol");
	if (alg > 0xff)
		CHECKM(ISC_R_RANGE, "key algorithm");
	keystruct.flags = (isc_uint16_t)flags;
	keystruct.protocol = (isc_uint8_t)proto;
	keystruct.algorithm = (isc_uint8_t)alg;

	isc_buffer_init(&keydatabuf, keydata, sizeof(keydata));
	isc_buffer_init(&rrdatabuf, rrdata, sizeof(rrdata));

	keystr = cfg_obj_asstring(cfg_tuple_get(key, "key"));
	CHECK(isc_base64_decodestring(keystr, &keydatabuf));
	isc_buffer_usedregion(&keydatabuf, &r);
	keystruct.datalen = r.length;
	keystruct.data = r.base;

	if ((keystruct.algorithm == DST_ALG_RSASHA1 ||
	     keystruct.algorithm == DST_ALG_RSAMD5) &&
	    r.length > 1 && r.base[0] == 1 && r.base[1] == 3)
		cfg_obj_log(key, ns_g_lctx, ISC_LOG_WARNING,
			    "trusted key '%s' has a weak exponent",
			    keynamestr);

	CHECK(dns_rdata_fromstruct(NULL,
				   keystruct.common.rdclass,
				   keystruct.common.rdtype,
				   &keystruct, &rrdatabuf));
	dns_fixedname_init(&fkeyname);
	isc_buffer_init(&namebuf, keynamestr, strlen(keynamestr));
	isc_buffer_add(&namebuf, strlen(keynamestr));
	CHECK(dns_name_fromtext(keyname, &namebuf,
				dns_rootname, ISC_FALSE,
				NULL));
	CHECK(dst_key_fromdns(keyname, viewclass, &rrdatabuf,
			      mctx, &dstkey));

	CHECK(dns_keytable_add(keytable, &dstkey));
	INSIST(dstkey == NULL);
	return (ISC_R_SUCCESS);

 cleanup:
	if (result == DST_R_NOCRYPTO) {
		cfg_obj_log(key, ns_g_lctx, ISC_LOG_ERROR,
			    "ignoring trusted key for '%s': no crypto support",
			    keynamestr);
		result = ISC_R_SUCCESS;
	} else {
		cfg_obj_log(key, ns_g_lctx, ISC_LOG_ERROR,
			    "configuring trusted key for '%s': %s",
			    keynamestr, isc_result_totext(result));
		result = ISC_R_FAILURE;
	}

	if (dstkey != NULL)
		dst_key_free(&dstkey);

	return (result);
}

/*%
 * Configure DNSSEC keys for a view.  Currently used only for
 * the security roots.
 *
 * The per-view configuration values and the server-global defaults are read
 * from 'vconfig' and 'config'.	 The variable to be configured is '*target'.
 */
static isc_result_t
configure_view_dnsseckeys(const cfg_obj_t *vconfig, const cfg_obj_t *config,
			  isc_mem_t *mctx, dns_keytable_t **target)
{
	isc_result_t result;
	const cfg_obj_t *keys = NULL;
	const cfg_obj_t *voptions = NULL;
	const cfg_listelt_t *element, *element2;
	const cfg_obj_t *keylist;
	const cfg_obj_t *key;
	dns_keytable_t *keytable = NULL;

	CHECK(dns_keytable_create(mctx, &keytable));

	if (vconfig != NULL)
		voptions = cfg_tuple_get(vconfig, "options");

	keys = NULL;
	if (voptions != NULL)
		(void)cfg_map_get(voptions, "trusted-keys", &keys);
	if (keys == NULL)
		(void)cfg_map_get(config, "trusted-keys", &keys);

	for (element = cfg_list_first(keys);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		keylist = cfg_listelt_value(element);
		for (element2 = cfg_list_first(keylist);
		     element2 != NULL;
		     element2 = cfg_list_next(element2))
		{
			key = cfg_listelt_value(element2);
			CHECK(configure_view_dnsseckey(vconfig, key,
						       keytable, mctx));
		}
	}

	dns_keytable_detach(target);
	*target = keytable; /* Transfer ownership. */
	keytable = NULL;
	result = ISC_R_SUCCESS;

 cleanup:
	return (result);
}

static isc_result_t
mustbesecure(const cfg_obj_t *mbs, dns_resolver_t *resolver)
{
	const cfg_listelt_t *element;
	const cfg_obj_t *obj;
	const char *str;
	dns_fixedname_t fixed;
	dns_name_t *name;
	isc_boolean_t value;
	isc_result_t result;
	isc_buffer_t b;

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	for (element = cfg_list_first(mbs);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		obj = cfg_listelt_value(element);
		str = cfg_obj_asstring(cfg_tuple_get(obj, "name"));
		isc_buffer_init(&b, str, strlen(str));
		isc_buffer_add(&b, strlen(str));
		CHECK(dns_name_fromtext(name, &b, dns_rootname,
					ISC_FALSE, NULL));
		value = cfg_obj_asboolean(cfg_tuple_get(obj, "value"));
		CHECK(dns_resolver_setmustbesecure(resolver, name, value));
	}

	result = ISC_R_SUCCESS;

 cleanup:
	return (result);
}

/*%
 * Get a dispatch appropriate for the resolver of a given view.
 */
static isc_result_t
get_view_querysource_dispatch(const cfg_obj_t **maps,
			      int af, dns_dispatch_t **dispatchp,
			      isc_boolean_t is_firstview)
{
	isc_result_t result;
	dns_dispatch_t *disp;
	isc_sockaddr_t sa;
	unsigned int attrs, attrmask;
	const cfg_obj_t *obj = NULL;
	unsigned int maxdispatchbuffers;

	/*
	 * Make compiler happy.
	 */
	result = ISC_R_FAILURE;

	switch (af) {
	case AF_INET:
		result = ns_config_get(maps, "query-source", &obj);
		INSIST(result == ISC_R_SUCCESS);
		break;
	case AF_INET6:
		result = ns_config_get(maps, "query-source-v6", &obj);
		INSIST(result == ISC_R_SUCCESS);
		break;
	default:
		INSIST(0);
	}

	sa = *(cfg_obj_assockaddr(obj));
	INSIST(isc_sockaddr_pf(&sa) == af);

	/*
	 * If we don't support this address family, we're done!
	 */
	switch (af) {
	case AF_INET:
		result = isc_net_probeipv4();
		break;
	case AF_INET6:
		result = isc_net_probeipv6();
		break;
	default:
		INSIST(0);
	}
	if (result != ISC_R_SUCCESS)
		return (ISC_R_SUCCESS);

	/*
	 * Try to find a dispatcher that we can share.
	 */
	attrs = 0;
	attrs |= DNS_DISPATCHATTR_UDP;
	switch (af) {
	case AF_INET:
		attrs |= DNS_DISPATCHATTR_IPV4;
		break;
	case AF_INET6:
		attrs |= DNS_DISPATCHATTR_IPV6;
		break;
	}
	if (isc_sockaddr_getport(&sa) == 0) {
		attrs |= DNS_DISPATCHATTR_EXCLUSIVE;
		maxdispatchbuffers = 4096;
	} else {
		INSIST(obj != NULL);
		if (is_firstview) {
			cfg_obj_log(obj, ns_g_lctx, ISC_LOG_INFO,
				    "using specific query-source port "
				    "suppresses port randomization and can be "
				    "insecure.");
		}
		maxdispatchbuffers = 1000;
	}

	attrmask = 0;
	attrmask |= DNS_DISPATCHATTR_UDP;
	attrmask |= DNS_DISPATCHATTR_TCP;
	attrmask |= DNS_DISPATCHATTR_IPV4;
	attrmask |= DNS_DISPATCHATTR_IPV6;

	disp = NULL;
	result = dns_dispatch_getudp(ns_g_dispatchmgr, ns_g_socketmgr,
				     ns_g_taskmgr, &sa, 4096,
				     maxdispatchbuffers, 32768, 16411, 16433,
				     attrs, attrmask, &disp);
	if (result != ISC_R_SUCCESS) {
		isc_sockaddr_t any;
		char buf[ISC_SOCKADDR_FORMATSIZE];

		switch (af) {
		case AF_INET:
			isc_sockaddr_any(&any);
			break;
		case AF_INET6:
			isc_sockaddr_any6(&any);
			break;
		}
		if (isc_sockaddr_equal(&sa, &any))
			return (ISC_R_SUCCESS);
		isc_sockaddr_format(&sa, buf, sizeof(buf));
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "could not get query source dispatcher (%s)",
			      buf);
		return (result);
	}

	*dispatchp = disp;

	return (ISC_R_SUCCESS);
}

static isc_result_t
configure_order(dns_order_t *order, const cfg_obj_t *ent) {
	dns_rdataclass_t rdclass;
	dns_rdatatype_t rdtype;
	const cfg_obj_t *obj;
	dns_fixedname_t fixed;
	unsigned int mode = 0;
	const char *str;
	isc_buffer_t b;
	isc_result_t result;
	isc_boolean_t addroot;

	result = ns_config_getclass(cfg_tuple_get(ent, "class"),
				    dns_rdataclass_any, &rdclass);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = ns_config_gettype(cfg_tuple_get(ent, "type"),
				   dns_rdatatype_any, &rdtype);
	if (result != ISC_R_SUCCESS)
		return (result);

	obj = cfg_tuple_get(ent, "name");
	if (cfg_obj_isstring(obj))
		str = cfg_obj_asstring(obj);
	else
		str = "*";
	addroot = ISC_TF(strcmp(str, "*") == 0);
	isc_buffer_init(&b, str, strlen(str));
	isc_buffer_add(&b, strlen(str));
	dns_fixedname_init(&fixed);
	result = dns_name_fromtext(dns_fixedname_name(&fixed), &b,
				   dns_rootname, ISC_FALSE, NULL);
	if (result != ISC_R_SUCCESS)
		return (result);

	obj = cfg_tuple_get(ent, "ordering");
	INSIST(cfg_obj_isstring(obj));
	str = cfg_obj_asstring(obj);
	if (!strcasecmp(str, "fixed"))
		mode = DNS_RDATASETATTR_FIXEDORDER;
	else if (!strcasecmp(str, "random"))
		mode = DNS_RDATASETATTR_RANDOMIZE;
	else if (!strcasecmp(str, "cyclic"))
		mode = 0;
	else
		INSIST(0);

	/*
	 * "*" should match everything including the root (BIND 8 compat).
	 * As dns_name_matcheswildcard(".", "*.") returns FALSE add a
	 * explicit entry for "." when the name is "*".
	 */
	if (addroot) {
		result = dns_order_add(order, dns_rootname,
				       rdtype, rdclass, mode);
		if (result != ISC_R_SUCCESS)
			return (result);
	}

	return (dns_order_add(order, dns_fixedname_name(&fixed),
			      rdtype, rdclass, mode));
}

static isc_result_t
configure_peer(const cfg_obj_t *cpeer, isc_mem_t *mctx, dns_peer_t **peerp) {
	isc_netaddr_t na;
	dns_peer_t *peer;
	const cfg_obj_t *obj;
	const char *str;
	isc_result_t result;
	unsigned int prefixlen;

	cfg_obj_asnetprefix(cfg_map_getname(cpeer), &na, &prefixlen);

	peer = NULL;
	result = dns_peer_newprefix(mctx, &na, prefixlen, &peer);
	if (result != ISC_R_SUCCESS)
		return (result);

	obj = NULL;
	(void)cfg_map_get(cpeer, "bogus", &obj);
	if (obj != NULL)
		CHECK(dns_peer_setbogus(peer, cfg_obj_asboolean(obj)));

	obj = NULL;
	(void)cfg_map_get(cpeer, "provide-ixfr", &obj);
	if (obj != NULL)
		CHECK(dns_peer_setprovideixfr(peer, cfg_obj_asboolean(obj)));

	obj = NULL;
	(void)cfg_map_get(cpeer, "request-ixfr", &obj);
	if (obj != NULL)
		CHECK(dns_peer_setrequestixfr(peer, cfg_obj_asboolean(obj)));

	obj = NULL;
	(void)cfg_map_get(cpeer, "request-nsid", &obj);
	if (obj != NULL)
		CHECK(dns_peer_setrequestnsid(peer, cfg_obj_asboolean(obj)));

	obj = NULL;
	(void)cfg_map_get(cpeer, "edns", &obj);
	if (obj != NULL)
		CHECK(dns_peer_setsupportedns(peer, cfg_obj_asboolean(obj)));

	obj = NULL;
	(void)cfg_map_get(cpeer, "edns-udp-size", &obj);
	if (obj != NULL) {
		isc_uint32_t udpsize = cfg_obj_asuint32(obj);
		if (udpsize < 512)
			udpsize = 512;
		if (udpsize > 4096)
			udpsize = 4096;
		CHECK(dns_peer_setudpsize(peer, (isc_uint16_t)udpsize));
	}

	obj = NULL;
	(void)cfg_map_get(cpeer, "max-udp-size", &obj);
	if (obj != NULL) {
		isc_uint32_t udpsize = cfg_obj_asuint32(obj);
		if (udpsize < 512)
			udpsize = 512;
		if (udpsize > 4096)
			udpsize = 4096;
		CHECK(dns_peer_setmaxudp(peer, (isc_uint16_t)udpsize));
	}

	obj = NULL;
	(void)cfg_map_get(cpeer, "transfers", &obj);
	if (obj != NULL)
		CHECK(dns_peer_settransfers(peer, cfg_obj_asuint32(obj)));

	obj = NULL;
	(void)cfg_map_get(cpeer, "transfer-format", &obj);
	if (obj != NULL) {
		str = cfg_obj_asstring(obj);
		if (strcasecmp(str, "many-answers") == 0)
			CHECK(dns_peer_settransferformat(peer,
							 dns_many_answers));
		else if (strcasecmp(str, "one-answer") == 0)
			CHECK(dns_peer_settransferformat(peer,
							 dns_one_answer));
		else
			INSIST(0);
	}

	obj = NULL;
	(void)cfg_map_get(cpeer, "keys", &obj);
	if (obj != NULL) {
		result = dns_peer_setkeybycharp(peer, cfg_obj_asstring(obj));
		if (result != ISC_R_SUCCESS)
			goto cleanup;
	}

	obj = NULL;
	if (na.family == AF_INET)
		(void)cfg_map_get(cpeer, "transfer-source", &obj);
	else
		(void)cfg_map_get(cpeer, "transfer-source-v6", &obj);
	if (obj != NULL) {
		result = dns_peer_settransfersource(peer,
						    cfg_obj_assockaddr(obj));
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		ns_add_reserved_dispatch(ns_g_server, cfg_obj_assockaddr(obj));
	}

	obj = NULL;
	if (na.family == AF_INET)
		(void)cfg_map_get(cpeer, "notify-source", &obj);
	else
		(void)cfg_map_get(cpeer, "notify-source-v6", &obj);
	if (obj != NULL) {
		result = dns_peer_setnotifysource(peer,
						  cfg_obj_assockaddr(obj));
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		ns_add_reserved_dispatch(ns_g_server, cfg_obj_assockaddr(obj));
	}

	obj = NULL;
	if (na.family == AF_INET)
		(void)cfg_map_get(cpeer, "query-source", &obj);
	else
		(void)cfg_map_get(cpeer, "query-source-v6", &obj);
	if (obj != NULL) {
		result = dns_peer_setquerysource(peer,
						 cfg_obj_assockaddr(obj));
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		ns_add_reserved_dispatch(ns_g_server, cfg_obj_assockaddr(obj));
	}

	*peerp = peer;
	return (ISC_R_SUCCESS);

 cleanup:
	dns_peer_detach(&peer);
	return (result);
}

static isc_result_t
disable_algorithms(const cfg_obj_t *disabled, dns_resolver_t *resolver) {
	isc_result_t result;
	const cfg_obj_t *algorithms;
	const cfg_listelt_t *element;
	const char *str;
	dns_fixedname_t fixed;
	dns_name_t *name;
	isc_buffer_t b;

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	str = cfg_obj_asstring(cfg_tuple_get(disabled, "name"));
	isc_buffer_init(&b, str, strlen(str));
	isc_buffer_add(&b, strlen(str));
	CHECK(dns_name_fromtext(name, &b, dns_rootname, ISC_FALSE, NULL));

	algorithms = cfg_tuple_get(disabled, "algorithms");
	for (element = cfg_list_first(algorithms);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		isc_textregion_t r;
		dns_secalg_t alg;

		DE_CONST(cfg_obj_asstring(cfg_listelt_value(element)), r.base);
		r.length = strlen(r.base);

		result = dns_secalg_fromtext(&alg, &r);
		if (result != ISC_R_SUCCESS) {
			isc_uint8_t ui;
			result = isc_parse_uint8(&ui, r.base, 10);
			alg = ui;
		}
		if (result != ISC_R_SUCCESS) {
			cfg_obj_log(cfg_listelt_value(element),
				    ns_g_lctx, ISC_LOG_ERROR,
				    "invalid algorithm");
			CHECK(result);
		}
		CHECK(dns_resolver_disable_algorithm(resolver, name, alg));
	}
 cleanup:
	return (result);
}

static isc_boolean_t
on_disable_list(const cfg_obj_t *disablelist, dns_name_t *zonename) {
	const cfg_listelt_t *element;
	dns_fixedname_t fixed;
	dns_name_t *name;
	isc_result_t result;
	const cfg_obj_t *value;
	const char *str;
	isc_buffer_t b;

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);

	for (element = cfg_list_first(disablelist);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		value = cfg_listelt_value(element);
		str = cfg_obj_asstring(value);
		isc_buffer_init(&b, str, strlen(str));
		isc_buffer_add(&b, strlen(str));
		result = dns_name_fromtext(name, &b, dns_rootname,
					   ISC_TRUE, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		if (dns_name_equal(name, zonename))
			return (ISC_TRUE);
	}
	return (ISC_FALSE);
}

static void
check_dbtype(dns_zone_t **zonep, unsigned int dbtypec, const char **dbargv,
	     isc_mem_t *mctx)
{
	char **argv = NULL;
	unsigned int i;
	isc_result_t result;

	result = dns_zone_getdbtype(*zonep, &argv, mctx);
	if (result != ISC_R_SUCCESS) {
		dns_zone_detach(zonep);
		return;
	}

	/*
	 * Check that all the arguments match.
	 */
	for (i = 0; i < dbtypec; i++)
		if (argv[i] == NULL || strcmp(argv[i], dbargv[i]) != 0) {
			dns_zone_detach(zonep);
			break;
		}

	/*
	 * Check that there are not extra arguments.
	 */
	if (i == dbtypec && argv[i] != NULL)
		dns_zone_detach(zonep);
	isc_mem_free(mctx, argv);
}

static isc_result_t
setquerystats(dns_zone_t *zone, isc_mem_t *mctx, isc_boolean_t on) {
	isc_result_t result;
	isc_stats_t *zoneqrystats;

	zoneqrystats = NULL;
	if (on) {
		result = isc_stats_create(mctx, &zoneqrystats,
					  dns_nsstatscounter_max);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	dns_zone_setrequeststats(zone, zoneqrystats);
	if (zoneqrystats != NULL)
		isc_stats_detach(&zoneqrystats);

	return (ISC_R_SUCCESS);
}

static isc_boolean_t
cache_reusable(dns_view_t *originview, dns_view_t *view,
	       isc_boolean_t new_zero_no_soattl)
{
	if (originview->checknames != view->checknames ||
	    dns_resolver_getzeronosoattl(originview->resolver) !=
	    new_zero_no_soattl ||
	    originview->acceptexpired != view->acceptexpired ||
	    originview->enablevalidation != view->enablevalidation ||
	    originview->maxcachettl != view->maxcachettl ||
	    originview->maxncachettl != view->maxncachettl) {
		return (ISC_FALSE);
	}

	return (ISC_TRUE);
}

/*
 * Configure 'view' according to 'vconfig', taking defaults from 'config'
 * where values are missing in 'vconfig'.
 *
 * When configuring the default view, 'vconfig' will be NULL and the
 * global defaults in 'config' used exclusively.
 */
static isc_result_t
configure_view(dns_view_t *view, const cfg_obj_t *config,
	       const cfg_obj_t *vconfig, isc_mem_t *mctx,
	       cfg_aclconfctx_t *actx, isc_boolean_t need_hints)
{
	const cfg_obj_t *maps[4];
	const cfg_obj_t *cfgmaps[3];
	const cfg_obj_t *options = NULL;
	const cfg_obj_t *voptions = NULL;
	const cfg_obj_t *forwardtype;
	const cfg_obj_t *forwarders;
	const cfg_obj_t *alternates;
	const cfg_obj_t *zonelist;
#ifdef DLZ
	const cfg_obj_t *dlz;
	unsigned int dlzargc;
	char **dlzargv;
#endif
	const cfg_obj_t *disabled;
	const cfg_obj_t *obj;
	const cfg_listelt_t *element;
	in_port_t port;
	dns_cache_t *cache = NULL;
	isc_result_t result;
	isc_uint32_t max_adb_size;
	isc_uint32_t max_cache_size;
	isc_uint32_t max_acache_size;
	isc_uint32_t lame_ttl;
	dns_tsig_keyring_t *ring;
	dns_view_t *pview = NULL;	/* Production view */
	isc_mem_t *cmctx;
	dns_dispatch_t *dispatch4 = NULL;
	dns_dispatch_t *dispatch6 = NULL;
	isc_boolean_t reused_cache = ISC_FALSE;
	int i;
	const char *str;
	dns_order_t *order = NULL;
	isc_uint32_t udpsize;
	unsigned int resopts = 0;
	dns_zone_t *zone = NULL;
	isc_uint32_t max_clients_per_query;
	const char *sep = ": view ";
	const char *viewname = view->name;
	const char *forview = " for view ";
	isc_boolean_t rfc1918;
	isc_boolean_t empty_zones_enable;
	const cfg_obj_t *disablelist = NULL;
	isc_stats_t *resstats = NULL;
	dns_stats_t *resquerystats = NULL;
	isc_boolean_t zero_no_soattl;

	REQUIRE(DNS_VIEW_VALID(view));

	cmctx = NULL;

	if (config != NULL)
		(void)cfg_map_get(config, "options", &options);

	i = 0;
	if (vconfig != NULL) {
		voptions = cfg_tuple_get(vconfig, "options");
		maps[i++] = voptions;
	}
	if (options != NULL)
		maps[i++] = options;
	maps[i++] = ns_g_defaults;
	maps[i] = NULL;

	i = 0;
	if (voptions != NULL)
		cfgmaps[i++] = voptions;
	if (config != NULL)
		cfgmaps[i++] = config;
	cfgmaps[i] = NULL;

	if (!strcmp(viewname, "_default")) {
		sep = "";
		viewname = "";
		forview = "";
	}

	/*
	 * Set the view's port number for outgoing queries.
	 */
	CHECKM(ns_config_getport(config, &port), "port");
	dns_view_setdstport(view, port);

	/*
	 * Create additional cache for this view and zones under the view
	 * if explicitly enabled.
	 * XXX950 default to on.
	 */
	obj = NULL;
	(void)ns_config_get(maps, "acache-enable", &obj);
	if (obj != NULL && cfg_obj_asboolean(obj)) {
		cmctx = NULL;
		CHECK(isc_mem_create(0, 0, &cmctx));
		CHECK(dns_acache_create(&view->acache, cmctx, ns_g_taskmgr,
					ns_g_timermgr));
		isc_mem_setname(cmctx, "acache", NULL);
		isc_mem_detach(&cmctx);
	}
	if (view->acache != NULL) {
		obj = NULL;
		result = ns_config_get(maps, "acache-cleaning-interval", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_acache_setcleaninginterval(view->acache,
					       cfg_obj_asuint32(obj) * 60);

		obj = NULL;
		result = ns_config_get(maps, "max-acache-size", &obj);
		INSIST(result == ISC_R_SUCCESS);
		if (cfg_obj_isstring(obj)) {
			str = cfg_obj_asstring(obj);
			INSIST(strcasecmp(str, "unlimited") == 0);
			max_acache_size = ISC_UINT32_MAX;
		} else {
			isc_resourcevalue_t value;

			value = cfg_obj_asuint64(obj);
			if (value > ISC_UINT32_MAX) {
				cfg_obj_log(obj, ns_g_lctx, ISC_LOG_ERROR,
					    "'max-acache-size "
					    "%" ISC_PRINT_QUADFORMAT
					    "d' is too large",
					    value);
				result = ISC_R_RANGE;
				goto cleanup;
			}
			max_acache_size = (isc_uint32_t)value;
		}
		dns_acache_setcachesize(view->acache, max_acache_size);
	}

	CHECK(configure_view_acl(vconfig, config, "allow-query", actx,
				 ns_g_mctx, &view->queryacl));

	if (view->queryacl == NULL) {
		CHECK(configure_view_acl(NULL, ns_g_config, "allow-query", actx,
					 ns_g_mctx, &view->queryacl));
	}

	/*
	 * Configure the zones.
	 */
	zonelist = NULL;
	if (voptions != NULL)
		(void)cfg_map_get(voptions, "zone", &zonelist);
	else
		(void)cfg_map_get(config, "zone", &zonelist);
	for (element = cfg_list_first(zonelist);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const cfg_obj_t *zconfig = cfg_listelt_value(element);
		CHECK(configure_zone(config, zconfig, vconfig, mctx, view,
				     actx));
	}

#ifdef DLZ
	/*
	 * Create Dynamically Loadable Zone driver.
	 */
	dlz = NULL;
	if (voptions != NULL)
		(void)cfg_map_get(voptions, "dlz", &dlz);
	else
		(void)cfg_map_get(config, "dlz", &dlz);

	obj = NULL;
	if (dlz != NULL) {
		(void)cfg_map_get(cfg_tuple_get(dlz, "options"),
				  "database", &obj);
		if (obj != NULL) {
			char *s = isc_mem_strdup(mctx, cfg_obj_asstring(obj));
			if (s == NULL) {
				result = ISC_R_NOMEMORY;
				goto cleanup;
			}

			result = dns_dlzstrtoargv(mctx, s, &dlzargc, &dlzargv);
			if (result != ISC_R_SUCCESS) {
				isc_mem_free(mctx, s);
				goto cleanup;
			}

			obj = cfg_tuple_get(dlz, "name");
			result = dns_dlzcreate(mctx, cfg_obj_asstring(obj),
					       dlzargv[0], dlzargc, dlzargv,
					       &view->dlzdatabase);
			isc_mem_free(mctx, s);
			isc_mem_put(mctx, dlzargv, dlzargc * sizeof(*dlzargv));
			if (result != ISC_R_SUCCESS)
				goto cleanup;
		}
	}
#endif

	/*
	 * Obtain configuration parameters that affect the decision of whether
	 * we can reuse/share an existing cache.
	 */
	/* Check-names. */
	obj = NULL;
	result = ns_checknames_get(maps, "response", &obj);
	INSIST(result == ISC_R_SUCCESS);

	str = cfg_obj_asstring(obj);
	if (strcasecmp(str, "fail") == 0) {
		resopts |= DNS_RESOLVER_CHECKNAMES |
			DNS_RESOLVER_CHECKNAMESFAIL;
		view->checknames = ISC_TRUE;
	} else if (strcasecmp(str, "warn") == 0) {
		resopts |= DNS_RESOLVER_CHECKNAMES;
		view->checknames = ISC_FALSE;
	} else if (strcasecmp(str, "ignore") == 0) {
		view->checknames = ISC_FALSE;
	} else
		INSIST(0);

	obj = NULL;
	result = ns_config_get(maps, "zero-no-soa-ttl-cache", &obj);
	INSIST(result == ISC_R_SUCCESS);
	zero_no_soattl = cfg_obj_asboolean(obj);

	obj = NULL;
	result = ns_config_get(maps, "dnssec-accept-expired", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->acceptexpired = cfg_obj_asboolean(obj);

	obj = NULL;
	result = ns_config_get(maps, "dnssec-validation", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->enablevalidation = cfg_obj_asboolean(obj);

	obj = NULL;
	result = ns_config_get(maps, "max-cache-ttl", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->maxcachettl = cfg_obj_asuint32(obj);

	obj = NULL;
	result = ns_config_get(maps, "max-ncache-ttl", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->maxncachettl = cfg_obj_asuint32(obj);
	if (view->maxncachettl > 7 * 24 * 3600)
		view->maxncachettl = 7 * 24 * 3600;

	/*
	 * Configure the view's cache.  Try to reuse an existing
	 * cache if possible, otherwise create a new cache.
	 * Note that the ADB is not preserved in either case.
	 * When a matching view is found, the associated statistics are
	 * also retrieved and reused.
	 *
	 * XXX Determining when it is safe to reuse a cache is tricky.
	 * When the view's configuration changes, the cached data may become
	 * invalid because it reflects our old view of the world.  We check
	 * some of the configuration parameters that could invalidate the cache,
	 * but there are other configuration options that should be checked.
	 * For example, if a view uses a forwarder, changes in the forwarder
	 * configuration may invalidate the cache.  At the moment, it's the
	 * administrator's responsibility to ensure these configuration options
	 * don't invalidate reusing.
	 */
	result = dns_viewlist_find(&ns_g_server->viewlist,
				   view->name, view->rdclass,
				   &pview);
	if (result != ISC_R_NOTFOUND && result != ISC_R_SUCCESS)
		goto cleanup;
	if (pview != NULL) {
		if (cache_reusable(pview, view, zero_no_soattl)) {
			INSIST(pview->cache != NULL);
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_DEBUG(3),
				      "reusing existing cache");
			reused_cache = ISC_TRUE;
			dns_cache_attach(pview->cache, &cache);
		} else {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_DEBUG(1),
				      "cache cannot be reused for view %s "
				      "due to configuration parameter mismatch",
				      view->name);
		}
		dns_view_getresstats(pview, &resstats);
		dns_view_getresquerystats(pview, &resquerystats);
		dns_view_detach(&pview);
	}
	if (cache == NULL) {
		CHECK(isc_mem_create(0, 0, &cmctx));
		CHECK(dns_cache_create(cmctx, ns_g_taskmgr, ns_g_timermgr,
				       view->rdclass, "rbt", 0, NULL, &cache));
		isc_mem_setname(cmctx, "cache", NULL);
	}
	dns_view_setcache(view, cache);

	/*
	 * cache-file cannot be inherited if views are present, but this
	 * should be caught by the configuration checking stage.
	 */
	obj = NULL;
	result = ns_config_get(maps, "cache-file", &obj);
	if (result == ISC_R_SUCCESS && strcmp(view->name, "_bind") != 0) {
		CHECK(dns_cache_setfilename(cache, cfg_obj_asstring(obj)));
		if (!reused_cache)
			CHECK(dns_cache_load(cache));
	}

	obj = NULL;
	result = ns_config_get(maps, "cleaning-interval", &obj);
	INSIST(result == ISC_R_SUCCESS);
	dns_cache_setcleaninginterval(cache, cfg_obj_asuint32(obj) * 60);

	obj = NULL;
	result = ns_config_get(maps, "max-cache-size", &obj);
	INSIST(result == ISC_R_SUCCESS);
	if (cfg_obj_isstring(obj)) {
		str = cfg_obj_asstring(obj);
		INSIST(strcasecmp(str, "unlimited") == 0);
		max_cache_size = ISC_UINT32_MAX;
	} else {
		isc_resourcevalue_t value;
		value = cfg_obj_asuint64(obj);
		if (value > ISC_UINT32_MAX) {
			cfg_obj_log(obj, ns_g_lctx, ISC_LOG_ERROR,
				    "'max-cache-size "
				    "%" ISC_PRINT_QUADFORMAT "d' is too large",
				    value);
			result = ISC_R_RANGE;
			goto cleanup;
		}
		max_cache_size = (isc_uint32_t)value;
	}
	dns_cache_setcachesize(cache, max_cache_size);

	dns_cache_detach(&cache);

	/*
	 * Resolver.
	 *
	 * XXXRTH  Hardwired number of tasks.
	 */
	CHECK(get_view_querysource_dispatch(maps, AF_INET, &dispatch4,
					    ISC_TF(ISC_LIST_PREV(view, link)
						   == NULL)));
	CHECK(get_view_querysource_dispatch(maps, AF_INET6, &dispatch6,
					    ISC_TF(ISC_LIST_PREV(view, link)
						   == NULL)));
	if (dispatch4 == NULL && dispatch6 == NULL) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "unable to obtain neither an IPv4 nor"
				 " an IPv6 dispatch");
		result = ISC_R_UNEXPECTED;
		goto cleanup;
	}
	CHECK(dns_view_createresolver(view, ns_g_taskmgr, 31,
				      ns_g_socketmgr, ns_g_timermgr,
				      resopts, ns_g_dispatchmgr,
				      dispatch4, dispatch6));

	if (resstats == NULL) {
		CHECK(isc_stats_create(mctx, &resstats,
				       dns_resstatscounter_max));
	}
	dns_view_setresstats(view, resstats);
	if (resquerystats == NULL)
		CHECK(dns_rdatatypestats_create(mctx, &resquerystats));
	dns_view_setresquerystats(view, resquerystats);

	/*
	 * Set the ADB cache size to 1/8th of the max-cache-size.
	 */
	max_adb_size = 0;
	if (max_cache_size != 0) {
		max_adb_size = max_cache_size / 8;
		if (max_adb_size == 0)
			max_adb_size = 1;	/* Force minimum. */
	}
	dns_adb_setadbsize(view->adb, max_adb_size);

	/*
	 * Set resolver's lame-ttl.
	 */
	obj = NULL;
	result = ns_config_get(maps, "lame-ttl", &obj);
	INSIST(result == ISC_R_SUCCESS);
	lame_ttl = cfg_obj_asuint32(obj);
	if (lame_ttl > 1800)
		lame_ttl = 1800;
	dns_resolver_setlamettl(view->resolver, lame_ttl);

	/*
	 * Set the resolver's EDNS UDP size.
	 */
	obj = NULL;
	result = ns_config_get(maps, "edns-udp-size", &obj);
	INSIST(result == ISC_R_SUCCESS);
	udpsize = cfg_obj_asuint32(obj);
	if (udpsize < 512)
		udpsize = 512;
	if (udpsize > 4096)
		udpsize = 4096;
	dns_resolver_setudpsize(view->resolver, (isc_uint16_t)udpsize);

	/*
	 * Set the maximum UDP response size.
	 */
	obj = NULL;
	result = ns_config_get(maps, "max-udp-size", &obj);
	INSIST(result == ISC_R_SUCCESS);
	udpsize = cfg_obj_asuint32(obj);
	if (udpsize < 512)
		udpsize = 512;
	if (udpsize > 4096)
		udpsize = 4096;
	view->maxudp = udpsize;

	/*
	 * Set supported DNSSEC algorithms.
	 */
	dns_resolver_reset_algorithms(view->resolver);
	disabled = NULL;
	(void)ns_config_get(maps, "disable-algorithms", &disabled);
	if (disabled != NULL) {
		for (element = cfg_list_first(disabled);
		     element != NULL;
		     element = cfg_list_next(element))
			CHECK(disable_algorithms(cfg_listelt_value(element),
						 view->resolver));
	}

	/*
	 * A global or view "forwarders" option, if present,
	 * creates an entry for "." in the forwarding table.
	 */
	forwardtype = NULL;
	forwarders = NULL;
	(void)ns_config_get(maps, "forward", &forwardtype);
	(void)ns_config_get(maps, "forwarders", &forwarders);
	if (forwarders != NULL)
		CHECK(configure_forward(config, view, dns_rootname,
					forwarders, forwardtype));

	/*
	 * Dual Stack Servers.
	 */
	alternates = NULL;
	(void)ns_config_get(maps, "dual-stack-servers", &alternates);
	if (alternates != NULL)
		CHECK(configure_alternates(config, view, alternates));

	/*
	 * We have default hints for class IN if we need them.
	 */
	if (view->rdclass == dns_rdataclass_in && view->hints == NULL)
		dns_view_sethints(view, ns_g_server->in_roothints);

	/*
	 * If we still have no hints, this is a non-IN view with no
	 * "hints zone" configured.  Issue a warning, except if this
	 * is a root server.  Root servers never need to consult
	 * their hints, so it's no point requiring users to configure
	 * them.
	 */
	if (view->hints == NULL) {
		dns_zone_t *rootzone = NULL;
		(void)dns_view_findzone(view, dns_rootname, &rootzone);
		if (rootzone != NULL) {
			dns_zone_detach(&rootzone);
			need_hints = ISC_FALSE;
		}
		if (need_hints)
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_WARNING,
				      "no root hints for view '%s'",
				      view->name);
	}

	/*
	 * Configure the view's TSIG keys.
	 */
	ring = NULL;
	CHECK(ns_tsigkeyring_fromconfig(config, vconfig, view->mctx, &ring));
	dns_view_setkeyring(view, ring);

	/*
	 * Configure the view's peer list.
	 */
	{
		const cfg_obj_t *peers = NULL;
		const cfg_listelt_t *element;
		dns_peerlist_t *newpeers = NULL;

		(void)ns_config_get(cfgmaps, "server", &peers);
		CHECK(dns_peerlist_new(mctx, &newpeers));
		for (element = cfg_list_first(peers);
		     element != NULL;
		     element = cfg_list_next(element))
		{
			const cfg_obj_t *cpeer = cfg_listelt_value(element);
			dns_peer_t *peer;

			CHECK(configure_peer(cpeer, mctx, &peer));
			dns_peerlist_addpeer(newpeers, peer);
			dns_peer_detach(&peer);
		}
		dns_peerlist_detach(&view->peers);
		view->peers = newpeers; /* Transfer ownership. */
	}

	/*
	 *	Configure the views rrset-order.
	 */
	{
		const cfg_obj_t *rrsetorder = NULL;
		const cfg_listelt_t *element;

		(void)ns_config_get(maps, "rrset-order", &rrsetorder);
		CHECK(dns_order_create(mctx, &order));
		for (element = cfg_list_first(rrsetorder);
		     element != NULL;
		     element = cfg_list_next(element))
		{
			const cfg_obj_t *ent = cfg_listelt_value(element);

			CHECK(configure_order(order, ent));
		}
		if (view->order != NULL)
			dns_order_detach(&view->order);
		dns_order_attach(order, &view->order);
		dns_order_detach(&order);
	}
	/*
	 * Copy the aclenv object.
	 */
	dns_aclenv_copy(&view->aclenv, &ns_g_server->aclenv);

	/*
	 * Configure the "match-clients" and "match-destinations" ACL.
	 */
	CHECK(configure_view_acl(vconfig, config, "match-clients", actx,
				 ns_g_mctx, &view->matchclients));
	CHECK(configure_view_acl(vconfig, config, "match-destinations", actx,
				 ns_g_mctx, &view->matchdestinations));

	/*
	 * Configure the "match-recursive-only" option.
	 */
	obj = NULL;
	(void)ns_config_get(maps, "match-recursive-only", &obj);
	if (obj != NULL && cfg_obj_asboolean(obj))
		view->matchrecursiveonly = ISC_TRUE;
	else
		view->matchrecursiveonly = ISC_FALSE;

	/*
	 * Configure other configurable data.
	 */
	obj = NULL;
	result = ns_config_get(maps, "recursion", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->recursion = cfg_obj_asboolean(obj);

	obj = NULL;
	result = ns_config_get(maps, "auth-nxdomain", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->auth_nxdomain = cfg_obj_asboolean(obj);

	obj = NULL;
	result = ns_config_get(maps, "minimal-responses", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->minimalresponses = cfg_obj_asboolean(obj);

	obj = NULL;
	result = ns_config_get(maps, "transfer-format", &obj);
	INSIST(result == ISC_R_SUCCESS);
	str = cfg_obj_asstring(obj);
	if (strcasecmp(str, "many-answers") == 0)
		view->transfer_format = dns_many_answers;
	else if (strcasecmp(str, "one-answer") == 0)
		view->transfer_format = dns_one_answer;
	else
		INSIST(0);

	/*
	 * Set sources where additional data and CNAME/DNAME
	 * targets for authoritative answers may be found.
	 */
	obj = NULL;
	result = ns_config_get(maps, "additional-from-auth", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->additionalfromauth = cfg_obj_asboolean(obj);
	if (view->recursion && ! view->additionalfromauth) {
		cfg_obj_log(obj, ns_g_lctx, ISC_LOG_WARNING,
			    "'additional-from-auth no' is only supported "
			    "with 'recursion no'");
		view->additionalfromauth = ISC_TRUE;
	}

	obj = NULL;
	result = ns_config_get(maps, "additional-from-cache", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->additionalfromcache = cfg_obj_asboolean(obj);
	if (view->recursion && ! view->additionalfromcache) {
		cfg_obj_log(obj, ns_g_lctx, ISC_LOG_WARNING,
			    "'additional-from-cache no' is only supported "
			    "with 'recursion no'");
		view->additionalfromcache = ISC_TRUE;
	}

	/*
	 * Set "allow-query-cache", "allow-query-cache-on",
	 * "allow-recursion", and "allow-recursion-on" acls if
	 * configured in named.conf.
	 */
	CHECK(configure_view_acl(vconfig, config, "allow-query-cache",
				 actx, ns_g_mctx, &view->cacheacl));
	CHECK(configure_view_acl(vconfig, config, "allow-query-cache-on",
				 actx, ns_g_mctx, &view->cacheonacl));
	if (view->cacheonacl == NULL)
		CHECK(configure_view_acl(NULL, ns_g_config,
					 "allow-query-cache-on", actx,
					 ns_g_mctx, &view->cacheonacl));
	if (strcmp(view->name, "_bind") != 0) {
		CHECK(configure_view_acl(vconfig, config, "allow-recursion",
					 actx, ns_g_mctx,
					 &view->recursionacl));
		CHECK(configure_view_acl(vconfig, config, "allow-recursion-on",
					 actx, ns_g_mctx,
					 &view->recursiononacl));
	}

	/*
	 * "allow-query-cache" inherits from "allow-recursion" if set,
	 * otherwise from "allow-query" if set.
	 * "allow-recursion" inherits from "allow-query-cache" if set,
	 * otherwise from "allow-query" if set.
	 */
	if (view->cacheacl == NULL && view->recursionacl != NULL)
		dns_acl_attach(view->recursionacl, &view->cacheacl);
	if (view->cacheacl == NULL && view->recursion)
		CHECK(configure_view_acl(vconfig, config, "allow-query",
					 actx, ns_g_mctx, &view->cacheacl));
	if (view->recursion &&
	    view->recursionacl == NULL && view->cacheacl != NULL)
		dns_acl_attach(view->cacheacl, &view->recursionacl);

	/*
	 * Set default "allow-recursion", "allow-recursion-on" and
	 * "allow-query-cache" acls.
	 */
	if (view->recursionacl == NULL && view->recursion)
		CHECK(configure_view_acl(NULL, ns_g_config,
					 "allow-recursion",
					 actx, ns_g_mctx,
					 &view->recursionacl));
	if (view->recursiononacl == NULL && view->recursion)
		CHECK(configure_view_acl(NULL, ns_g_config,
					 "allow-recursion-on",
					 actx, ns_g_mctx,
					 &view->recursiononacl));
	if (view->cacheacl == NULL) {
		if (view->recursion)
			CHECK(configure_view_acl(NULL, ns_g_config,
						 "allow-query-cache", actx,
						 ns_g_mctx, &view->cacheacl));
		else
			CHECK(dns_acl_none(ns_g_mctx, &view->cacheacl));
	}

	/*
	 * Configure sortlist, if set
	 */
	CHECK(configure_view_sortlist(vconfig, config, actx, ns_g_mctx,
				      &view->sortlist));

	/*
	 * Configure default allow-transfer, allow-notify, allow-update
	 * and allow-update-forwarding ACLs, if set, so they can be
	 * inherited by zones.
	 */
	if (view->notifyacl == NULL)
		CHECK(configure_view_acl(NULL, ns_g_config,
					 "allow-notify", actx,
					 ns_g_mctx, &view->notifyacl));
	if (view->transferacl == NULL)
		CHECK(configure_view_acl(NULL, ns_g_config,
					 "allow-transfer", actx,
					 ns_g_mctx, &view->transferacl));
	if (view->updateacl == NULL)
		CHECK(configure_view_acl(NULL, ns_g_config,
					 "allow-update", actx,
					 ns_g_mctx, &view->updateacl));
	if (view->upfwdacl == NULL)
		CHECK(configure_view_acl(NULL, ns_g_config,
					 "allow-update-forwarding", actx,
					 ns_g_mctx, &view->upfwdacl));

	obj = NULL;
	result = ns_config_get(maps, "request-ixfr", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->requestixfr = cfg_obj_asboolean(obj);

	obj = NULL;
	result = ns_config_get(maps, "provide-ixfr", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->provideixfr = cfg_obj_asboolean(obj);

	obj = NULL;
	result = ns_config_get(maps, "request-nsid", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->requestnsid = cfg_obj_asboolean(obj);

	obj = NULL;
	result = ns_config_get(maps, "max-clients-per-query", &obj);
	INSIST(result == ISC_R_SUCCESS);
	max_clients_per_query = cfg_obj_asuint32(obj);

	obj = NULL;
	result = ns_config_get(maps, "clients-per-query", &obj);
	INSIST(result == ISC_R_SUCCESS);
	dns_resolver_setclientsperquery(view->resolver,
					cfg_obj_asuint32(obj),
					max_clients_per_query);

	obj = NULL;
	result = ns_config_get(maps, "dnssec-enable", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->enablednssec = cfg_obj_asboolean(obj);

	obj = NULL;
	result = ns_config_get(maps, "dnssec-lookaside", &obj);
	if (result == ISC_R_SUCCESS) {
		for (element = cfg_list_first(obj);
		     element != NULL;
		     element = cfg_list_next(element))
		{
			const char *str;
			isc_buffer_t b;
			dns_name_t *dlv;

			obj = cfg_listelt_value(element);
#if 0
			dns_fixedname_t fixed;
			dns_name_t *name;

			/*
			 * When we support multiple dnssec-lookaside
			 * entries this is how to find the domain to be
			 * checked. XXXMPA
			 */
			dns_fixedname_init(&fixed);
			name = dns_fixedname_name(&fixed);
			str = cfg_obj_asstring(cfg_tuple_get(obj,
							     "domain"));
			isc_buffer_init(&b, str, strlen(str));
			isc_buffer_add(&b, strlen(str));
			CHECK(dns_name_fromtext(name, &b, dns_rootname,
						ISC_TRUE, NULL));
#endif
			str = cfg_obj_asstring(cfg_tuple_get(obj,
							     "trust-anchor"));
			isc_buffer_init(&b, str, strlen(str));
			isc_buffer_add(&b, strlen(str));
			dlv = dns_fixedname_name(&view->dlv_fixed);
			CHECK(dns_name_fromtext(dlv, &b, dns_rootname,
						ISC_TRUE, NULL));
			view->dlv = dns_fixedname_name(&view->dlv_fixed);
		}
	} else
		view->dlv = NULL;

	/*
	 * For now, there is only one kind of trusted keys, the
	 * "security roots".
	 */
	CHECK(configure_view_dnsseckeys(vconfig, config, mctx,
					&view->secroots));
	dns_resolver_resetmustbesecure(view->resolver);
	obj = NULL;
	result = ns_config_get(maps, "dnssec-must-be-secure", &obj);
	if (result == ISC_R_SUCCESS)
		CHECK(mustbesecure(obj, view->resolver));

	obj = NULL;
	result = ns_config_get(maps, "preferred-glue", &obj);
	if (result == ISC_R_SUCCESS) {
		str = cfg_obj_asstring(obj);
		if (strcasecmp(str, "a") == 0)
			view->preferred_glue = dns_rdatatype_a;
		else if (strcasecmp(str, "aaaa") == 0)
			view->preferred_glue = dns_rdatatype_aaaa;
		else
			view->preferred_glue = 0;
	} else
		view->preferred_glue = 0;

	obj = NULL;
	result = ns_config_get(maps, "root-delegation-only", &obj);
	if (result == ISC_R_SUCCESS) {
		dns_view_setrootdelonly(view, ISC_TRUE);
		if (!cfg_obj_isvoid(obj)) {
			dns_fixedname_t fixed;
			dns_name_t *name;
			isc_buffer_t b;
			const char *str;
			const cfg_obj_t *exclude;

			dns_fixedname_init(&fixed);
			name = dns_fixedname_name(&fixed);
			for (element = cfg_list_first(obj);
			     element != NULL;
			     element = cfg_list_next(element)) {
				exclude = cfg_listelt_value(element);
				str = cfg_obj_asstring(exclude);
				isc_buffer_init(&b, str, strlen(str));
				isc_buffer_add(&b, strlen(str));
				CHECK(dns_name_fromtext(name, &b, dns_rootname,
							ISC_FALSE, NULL));
				CHECK(dns_view_excludedelegationonly(view,
								     name));
			}
		}
	} else
		dns_view_setrootdelonly(view, ISC_FALSE);

	/*
	 * Setup automatic empty zones.  If recursion is off then
	 * they are disabled by default.
	 */
	obj = NULL;
	(void)ns_config_get(maps, "empty-zones-enable", &obj);
	(void)ns_config_get(maps, "disable-empty-zone", &disablelist);
	if (obj == NULL && disablelist == NULL &&
	    view->rdclass == dns_rdataclass_in) {
		rfc1918 = ISC_FALSE;
		empty_zones_enable = view->recursion;
	} else if (view->rdclass == dns_rdataclass_in) {
		rfc1918 = ISC_TRUE;
		if (obj != NULL)
			empty_zones_enable = cfg_obj_asboolean(obj);
		else
			empty_zones_enable = view->recursion;
	} else {
		rfc1918 = ISC_FALSE;
		empty_zones_enable = ISC_FALSE;
	}
	if (empty_zones_enable) {
		const char *empty;
		int empty_zone = 0;
		dns_fixedname_t fixed;
		dns_name_t *name;
		isc_buffer_t buffer;
		const char *str;
		char server[DNS_NAME_FORMATSIZE + 1];
		char contact[DNS_NAME_FORMATSIZE + 1];
		isc_boolean_t logit;
		const char *empty_dbtype[4] =
				    { "_builtin", "empty", NULL, NULL };
		int empty_dbtypec = 4;
		isc_boolean_t zonestats_on;

		dns_fixedname_init(&fixed);
		name = dns_fixedname_name(&fixed);

		obj = NULL;
		result = ns_config_get(maps, "empty-server", &obj);
		if (result == ISC_R_SUCCESS) {
			str = cfg_obj_asstring(obj);
			isc_buffer_init(&buffer, str, strlen(str));
			isc_buffer_add(&buffer, strlen(str));
			CHECK(dns_name_fromtext(name, &buffer, dns_rootname,
						ISC_FALSE, NULL));
			isc_buffer_init(&buffer, server, sizeof(server) - 1);
			CHECK(dns_name_totext(name, ISC_FALSE, &buffer));
			server[isc_buffer_usedlength(&buffer)] = 0;
			empty_dbtype[2] = server;
		} else
			empty_dbtype[2] = "@";

		obj = NULL;
		result = ns_config_get(maps, "empty-contact", &obj);
		if (result == ISC_R_SUCCESS) {
			str = cfg_obj_asstring(obj);
			isc_buffer_init(&buffer, str, strlen(str));
			isc_buffer_add(&buffer, strlen(str));
			CHECK(dns_name_fromtext(name, &buffer, dns_rootname,
						ISC_FALSE, NULL));
			isc_buffer_init(&buffer, contact, sizeof(contact) - 1);
			CHECK(dns_name_totext(name, ISC_FALSE, &buffer));
			contact[isc_buffer_usedlength(&buffer)] = 0;
			empty_dbtype[3] = contact;
		} else
			empty_dbtype[3] = ".";

		obj = NULL;
		result = ns_config_get(maps, "zone-statistics", &obj);
		INSIST(result == ISC_R_SUCCESS);
		zonestats_on = cfg_obj_asboolean(obj);

		logit = ISC_TRUE;
		for (empty = empty_zones[empty_zone].zone;
		     empty != NULL;
		     empty = empty_zones[++empty_zone].zone)
		{
			dns_forwarders_t *forwarders = NULL;
			dns_view_t *pview = NULL;

			isc_buffer_init(&buffer, empty, strlen(empty));
			isc_buffer_add(&buffer, strlen(empty));
			/*
			 * Look for zone on drop list.
			 */
			CHECK(dns_name_fromtext(name, &buffer, dns_rootname,
						ISC_FALSE, NULL));
			if (disablelist != NULL &&
			    on_disable_list(disablelist, name))
				continue;

			/*
			 * This zone already exists.
			 */
			(void)dns_view_findzone(view, name, &zone);
			if (zone != NULL) {
				CHECK(setquerystats(zone, mctx, zonestats_on));
				dns_zone_detach(&zone);
				continue;
			}

			/*
			 * If we would forward this name don't add a
			 * empty zone for it.
			 */
			result = dns_fwdtable_find(view->fwdtable, name,
						   &forwarders);
			if (result == ISC_R_SUCCESS &&
			    forwarders->fwdpolicy == dns_fwdpolicy_only)
				continue;

			if (!rfc1918 && empty_zones[empty_zone].rfc1918) {
				if (logit) {
					isc_log_write(ns_g_lctx,
						      NS_LOGCATEGORY_GENERAL,
						      NS_LOGMODULE_SERVER,
						      ISC_LOG_WARNING,
						      "Warning%s%s: "
						      "'empty-zones-enable/"
						      "disable-empty-zone' "
						      "not set: disabling "
						      "RFC 1918 empty zones",
						      sep, viewname);
					logit = ISC_FALSE;
				}
				continue;
			}

			/*
			 * See if we can re-use a existing zone.
			 */
			result = dns_viewlist_find(&ns_g_server->viewlist,
						   view->name, view->rdclass,
						   &pview);
			if (result != ISC_R_NOTFOUND &&
			    result != ISC_R_SUCCESS)
				goto cleanup;

			if (pview != NULL) {
				(void)dns_view_findzone(pview, name, &zone);
				dns_view_detach(&pview);
				if (zone != NULL)
					check_dbtype(&zone, empty_dbtypec,
						     empty_dbtype, mctx);
				if (zone != NULL) {
					dns_zone_setview(zone, view);
					CHECK(dns_view_addzone(view, zone));
					CHECK(setquerystats(zone, mctx,
							    zonestats_on));
					dns_zone_detach(&zone);
					continue;
				}
			}

			CHECK(dns_zone_create(&zone, mctx));
			CHECK(dns_zone_setorigin(zone, name));
			dns_zone_setview(zone, view);
			CHECK(dns_zonemgr_managezone(ns_g_server->zonemgr, zone));
			dns_zone_setclass(zone, view->rdclass);
			dns_zone_settype(zone, dns_zone_master);
			dns_zone_setstats(zone, ns_g_server->zonestats);
			CHECK(dns_zone_setdbtype(zone, empty_dbtypec,
						 empty_dbtype));
			if (view->queryacl != NULL)
				dns_zone_setqueryacl(zone, view->queryacl);
			if (view->queryonacl != NULL)
				dns_zone_setqueryonacl(zone, view->queryonacl);
			dns_zone_setdialup(zone, dns_dialuptype_no);
			dns_zone_setnotifytype(zone, dns_notifytype_no);
			dns_zone_setoption(zone, DNS_ZONEOPT_NOCHECKNS,
					   ISC_TRUE);
			CHECK(setquerystats(zone, mctx, zonestats_on));
			CHECK(dns_view_addzone(view, zone));
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
				      "automatic empty zone%s%s: %s",
				      sep, viewname,  empty);
			dns_zone_detach(&zone);
		}
	}

	result = ISC_R_SUCCESS;

 cleanup:
	if (zone != NULL)
		dns_zone_detach(&zone);
	if (dispatch4 != NULL)
		dns_dispatch_detach(&dispatch4);
	if (dispatch6 != NULL)
		dns_dispatch_detach(&dispatch6);
	if (resstats != NULL)
		isc_stats_detach(&resstats);
	if (resquerystats != NULL)
		dns_stats_detach(&resquerystats);
	if (order != NULL)
		dns_order_detach(&order);
	if (cmctx != NULL)
		isc_mem_detach(&cmctx);

	if (cache != NULL)
		dns_cache_detach(&cache);

	return (result);
}

static isc_result_t
configure_hints(dns_view_t *view, const char *filename) {
	isc_result_t result;
	dns_db_t *db;

	db = NULL;
	result = dns_rootns_create(view->mctx, view->rdclass, filename, &db);
	if (result == ISC_R_SUCCESS) {
		dns_view_sethints(view, db);
		dns_db_detach(&db);
	}

	return (result);
}

static isc_result_t
configure_alternates(const cfg_obj_t *config, dns_view_t *view,
		     const cfg_obj_t *alternates)
{
	const cfg_obj_t *portobj;
	const cfg_obj_t *addresses;
	const cfg_listelt_t *element;
	isc_result_t result = ISC_R_SUCCESS;
	in_port_t port;

	/*
	 * Determine which port to send requests to.
	 */
	if (ns_g_lwresdonly && ns_g_port != 0)
		port = ns_g_port;
	else
		CHECKM(ns_config_getport(config, &port), "port");

	if (alternates != NULL) {
		portobj = cfg_tuple_get(alternates, "port");
		if (cfg_obj_isuint32(portobj)) {
			isc_uint32_t val = cfg_obj_asuint32(portobj);
			if (val > ISC_UINT16_MAX) {
				cfg_obj_log(portobj, ns_g_lctx, ISC_LOG_ERROR,
					    "port '%u' out of range", val);
				return (ISC_R_RANGE);
			}
			port = (in_port_t) val;
		}
	}

	addresses = NULL;
	if (alternates != NULL)
		addresses = cfg_tuple_get(alternates, "addresses");

	for (element = cfg_list_first(addresses);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const cfg_obj_t *alternate = cfg_listelt_value(element);
		isc_sockaddr_t sa;

		if (!cfg_obj_issockaddr(alternate)) {
			dns_fixedname_t fixed;
			dns_name_t *name;
			const char *str = cfg_obj_asstring(cfg_tuple_get(
							   alternate, "name"));
			isc_buffer_t buffer;
			in_port_t myport = port;

			isc_buffer_init(&buffer, str, strlen(str));
			isc_buffer_add(&buffer, strlen(str));
			dns_fixedname_init(&fixed);
			name = dns_fixedname_name(&fixed);
			CHECK(dns_name_fromtext(name, &buffer, dns_rootname,
						ISC_FALSE, NULL));

			portobj = cfg_tuple_get(alternate, "port");
			if (cfg_obj_isuint32(portobj)) {
				isc_uint32_t val = cfg_obj_asuint32(portobj);
				if (val > ISC_UINT16_MAX) {
					cfg_obj_log(portobj, ns_g_lctx,
						    ISC_LOG_ERROR,
						    "port '%u' out of range",
						     val);
					return (ISC_R_RANGE);
				}
				myport = (in_port_t) val;
			}
			CHECK(dns_resolver_addalternate(view->resolver, NULL,
							name, myport));
			continue;
		}

		sa = *cfg_obj_assockaddr(alternate);
		if (isc_sockaddr_getport(&sa) == 0)
			isc_sockaddr_setport(&sa, port);
		CHECK(dns_resolver_addalternate(view->resolver, &sa,
						NULL, 0));
	}

 cleanup:
	return (result);
}

static isc_result_t
configure_forward(const cfg_obj_t *config, dns_view_t *view, dns_name_t *origin,
		  const cfg_obj_t *forwarders, const cfg_obj_t *forwardtype)
{
	const cfg_obj_t *portobj;
	const cfg_obj_t *faddresses;
	const cfg_listelt_t *element;
	dns_fwdpolicy_t fwdpolicy = dns_fwdpolicy_none;
	isc_sockaddrlist_t addresses;
	isc_sockaddr_t *sa;
	isc_result_t result;
	in_port_t port;

	ISC_LIST_INIT(addresses);

	/*
	 * Determine which port to send forwarded requests to.
	 */
	if (ns_g_lwresdonly && ns_g_port != 0)
		port = ns_g_port;
	else
		CHECKM(ns_config_getport(config, &port), "port");

	if (forwarders != NULL) {
		portobj = cfg_tuple_get(forwarders, "port");
		if (cfg_obj_isuint32(portobj)) {
			isc_uint32_t val = cfg_obj_asuint32(portobj);
			if (val > ISC_UINT16_MAX) {
				cfg_obj_log(portobj, ns_g_lctx, ISC_LOG_ERROR,
					    "port '%u' out of range", val);
				return (ISC_R_RANGE);
			}
			port = (in_port_t) val;
		}
	}

	faddresses = NULL;
	if (forwarders != NULL)
		faddresses = cfg_tuple_get(forwarders, "addresses");

	for (element = cfg_list_first(faddresses);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const cfg_obj_t *forwarder = cfg_listelt_value(element);
		sa = isc_mem_get(view->mctx, sizeof(isc_sockaddr_t));
		if (sa == NULL) {
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
		*sa = *cfg_obj_assockaddr(forwarder);
		if (isc_sockaddr_getport(sa) == 0)
			isc_sockaddr_setport(sa, port);
		ISC_LINK_INIT(sa, link);
		ISC_LIST_APPEND(addresses, sa, link);
	}

	if (ISC_LIST_EMPTY(addresses)) {
		if (forwardtype != NULL)
			cfg_obj_log(forwarders, ns_g_lctx, ISC_LOG_WARNING,
				    "no forwarders seen; disabling "
				    "forwarding");
		fwdpolicy = dns_fwdpolicy_none;
	} else {
		if (forwardtype == NULL)
			fwdpolicy = dns_fwdpolicy_first;
		else {
			const char *forwardstr = cfg_obj_asstring(forwardtype);
			if (strcasecmp(forwardstr, "first") == 0)
				fwdpolicy = dns_fwdpolicy_first;
			else if (strcasecmp(forwardstr, "only") == 0)
				fwdpolicy = dns_fwdpolicy_only;
			else
				INSIST(0);
		}
	}

	result = dns_fwdtable_add(view->fwdtable, origin, &addresses,
				  fwdpolicy);
	if (result != ISC_R_SUCCESS) {
		char namebuf[DNS_NAME_FORMATSIZE];
		dns_name_format(origin, namebuf, sizeof(namebuf));
		cfg_obj_log(forwarders, ns_g_lctx, ISC_LOG_WARNING,
			    "could not set up forwarding for domain '%s': %s",
			    namebuf, isc_result_totext(result));
		goto cleanup;
	}

	result = ISC_R_SUCCESS;

 cleanup:

	while (!ISC_LIST_EMPTY(addresses)) {
		sa = ISC_LIST_HEAD(addresses);
		ISC_LIST_UNLINK(addresses, sa, link);
		isc_mem_put(view->mctx, sa, sizeof(isc_sockaddr_t));
	}

	return (result);
}

/*
 * Create a new view and add it to the list.
 *
 * If 'vconfig' is NULL, create the default view.
 *
 * The view created is attached to '*viewp'.
 */
static isc_result_t
create_view(const cfg_obj_t *vconfig, dns_viewlist_t *viewlist,
	    dns_view_t **viewp)
{
	isc_result_t result;
	const char *viewname;
	dns_rdataclass_t viewclass;
	dns_view_t *view = NULL;

	if (vconfig != NULL) {
		const cfg_obj_t *classobj = NULL;

		viewname = cfg_obj_asstring(cfg_tuple_get(vconfig, "name"));
		classobj = cfg_tuple_get(vconfig, "class");
		result = ns_config_getclass(classobj, dns_rdataclass_in,
					    &viewclass);
	} else {
		viewname = "_default";
		viewclass = dns_rdataclass_in;
	}
	result = dns_viewlist_find(viewlist, viewname, viewclass, &view);
	if (result == ISC_R_SUCCESS)
		return (ISC_R_EXISTS);
	if (result != ISC_R_NOTFOUND)
		return (result);
	INSIST(view == NULL);

	result = dns_view_create(ns_g_mctx, viewclass, viewname, &view);
	if (result != ISC_R_SUCCESS)
		return (result);

	ISC_LIST_APPEND(*viewlist, view, link);
	dns_view_attach(view, viewp);
	return (ISC_R_SUCCESS);
}

/*
 * Configure or reconfigure a zone.
 */
static isc_result_t
configure_zone(const cfg_obj_t *config, const cfg_obj_t *zconfig,
	       const cfg_obj_t *vconfig, isc_mem_t *mctx, dns_view_t *view,
	       cfg_aclconfctx_t *aclconf)
{
	dns_view_t *pview = NULL;	/* Production view */
	dns_zone_t *zone = NULL;	/* New or reused zone */
	dns_zone_t *dupzone = NULL;
	const cfg_obj_t *options = NULL;
	const cfg_obj_t *zoptions = NULL;
	const cfg_obj_t *typeobj = NULL;
	const cfg_obj_t *forwarders = NULL;
	const cfg_obj_t *forwardtype = NULL;
	const cfg_obj_t *only = NULL;
	isc_result_t result;
	isc_result_t tresult;
	isc_buffer_t buffer;
	dns_fixedname_t fixorigin;
	dns_name_t *origin;
	const char *zname;
	dns_rdataclass_t zclass;
	const char *ztypestr;

	options = NULL;
	(void)cfg_map_get(config, "options", &options);

	zoptions = cfg_tuple_get(zconfig, "options");

	/*
	 * Get the zone origin as a dns_name_t.
	 */
	zname = cfg_obj_asstring(cfg_tuple_get(zconfig, "name"));
	isc_buffer_init(&buffer, zname, strlen(zname));
	isc_buffer_add(&buffer, strlen(zname));
	dns_fixedname_init(&fixorigin);
	CHECK(dns_name_fromtext(dns_fixedname_name(&fixorigin),
				&buffer, dns_rootname, ISC_FALSE, NULL));
	origin = dns_fixedname_name(&fixorigin);

	CHECK(ns_config_getclass(cfg_tuple_get(zconfig, "class"),
				 view->rdclass, &zclass));
	if (zclass != view->rdclass) {
		const char *vname = NULL;
		if (vconfig != NULL)
			vname = cfg_obj_asstring(cfg_tuple_get(vconfig,
							       "name"));
		else
			vname = "<default view>";

		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "zone '%s': wrong class for view '%s'",
			      zname, vname);
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	(void)cfg_map_get(zoptions, "type", &typeobj);
	if (typeobj == NULL) {
		cfg_obj_log(zconfig, ns_g_lctx, ISC_LOG_ERROR,
			    "zone '%s' 'type' not specified", zname);
		return (ISC_R_FAILURE);
	}
	ztypestr = cfg_obj_asstring(typeobj);

	/*
	 * "hints zones" aren't zones.  If we've got one,
	 * configure it and return.
	 */
	if (strcasecmp(ztypestr, "hint") == 0) {
		const cfg_obj_t *fileobj = NULL;
		if (cfg_map_get(zoptions, "file", &fileobj) != ISC_R_SUCCESS) {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
				      "zone '%s': 'file' not specified",
				      zname);
			result = ISC_R_FAILURE;
			goto cleanup;
		}
		if (dns_name_equal(origin, dns_rootname)) {
			const char *hintsfile = cfg_obj_asstring(fileobj);

			result = configure_hints(view, hintsfile);
			if (result != ISC_R_SUCCESS) {
				isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
					      NS_LOGMODULE_SERVER,
					      ISC_LOG_ERROR,
					      "could not configure root hints "
					      "from '%s': %s", hintsfile,
					      isc_result_totext(result));
				goto cleanup;
			}
			/*
			 * Hint zones may also refer to delegation only points.
			 */
			only = NULL;
			tresult = cfg_map_get(zoptions, "delegation-only",
					      &only);
			if (tresult == ISC_R_SUCCESS && cfg_obj_asboolean(only))
				CHECK(dns_view_adddelegationonly(view, origin));
		} else {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_WARNING,
				      "ignoring non-root hint zone '%s'",
				      zname);
			result = ISC_R_SUCCESS;
		}
		/* Skip ordinary zone processing. */
		goto cleanup;
	}

	/*
	 * "forward zones" aren't zones either.  Translate this syntax into
	 * the appropriate selective forwarding configuration and return.
	 */
	if (strcasecmp(ztypestr, "forward") == 0) {
		forwardtype = NULL;
		forwarders = NULL;

		(void)cfg_map_get(zoptions, "forward", &forwardtype);
		(void)cfg_map_get(zoptions, "forwarders", &forwarders);
		result = configure_forward(config, view, origin, forwarders,
					   forwardtype);
		goto cleanup;
	}

	/*
	 * "delegation-only zones" aren't zones either.
	 */
	if (strcasecmp(ztypestr, "delegation-only") == 0) {
		result = dns_view_adddelegationonly(view, origin);
		goto cleanup;
	}

	/*
	 * Check for duplicates in the new zone table.
	 */
	result = dns_view_findzone(view, origin, &dupzone);
	if (result == ISC_R_SUCCESS) {
		/*
		 * We already have this zone!
		 */
		cfg_obj_log(zconfig, ns_g_lctx, ISC_LOG_ERROR,
			    "zone '%s' already exists", zname);
		dns_zone_detach(&dupzone);
		result = ISC_R_EXISTS;
		goto cleanup;
	}
	INSIST(dupzone == NULL);

	/*
	 * See if we can reuse an existing zone.  This is
	 * only possible if all of these are true:
	 *   - The zone's view exists
	 *   - A zone with the right name exists in the view
	 *   - The zone is compatible with the config
	 *     options (e.g., an existing master zone cannot
	 *     be reused if the options specify a slave zone)
	 */
	result = dns_viewlist_find(&ns_g_server->viewlist,
				   view->name, view->rdclass,
				   &pview);
	if (result != ISC_R_NOTFOUND && result != ISC_R_SUCCESS)
		goto cleanup;
	if (pview != NULL)
		result = dns_view_findzone(pview, origin, &zone);
	if (result != ISC_R_NOTFOUND && result != ISC_R_SUCCESS)
		goto cleanup;
	if (zone != NULL && !ns_zone_reusable(zone, zconfig))
		dns_zone_detach(&zone);

	if (zone != NULL) {
		/*
		 * We found a reusable zone.  Make it use the
		 * new view.
		 */
		dns_zone_setview(zone, view);
		if (view->acache != NULL)
			dns_zone_setacache(zone, view->acache);
	} else {
		/*
		 * We cannot reuse an existing zone, we have
		 * to create a new one.
		 */
		CHECK(dns_zone_create(&zone, mctx));
		CHECK(dns_zone_setorigin(zone, origin));
		dns_zone_setview(zone, view);
		if (view->acache != NULL)
			dns_zone_setacache(zone, view->acache);
		CHECK(dns_zonemgr_managezone(ns_g_server->zonemgr, zone));
		dns_zone_setstats(zone, ns_g_server->zonestats);
	}

	/*
	 * If the zone contains a 'forwarders' statement, configure
	 * selective forwarding.
	 */
	forwarders = NULL;
	if (cfg_map_get(zoptions, "forwarders", &forwarders) == ISC_R_SUCCESS)
	{
		forwardtype = NULL;
		(void)cfg_map_get(zoptions, "forward", &forwardtype);
		CHECK(configure_forward(config, view, origin, forwarders,
					forwardtype));
	}

	/*
	 * Stub and forward zones may also refer to delegation only points.
	 */
	only = NULL;
	if (cfg_map_get(zoptions, "delegation-only", &only) == ISC_R_SUCCESS)
	{
		if (cfg_obj_asboolean(only))
			CHECK(dns_view_adddelegationonly(view, origin));
	}

	/*
	 * Configure the zone.
	 */
	CHECK(ns_zone_configure(config, vconfig, zconfig, aclconf, zone));

	/*
	 * Add the zone to its view in the new view list.
	 */
	CHECK(dns_view_addzone(view, zone));

 cleanup:
	if (zone != NULL)
		dns_zone_detach(&zone);
	if (pview != NULL)
		dns_view_detach(&pview);

	return (result);
}

/*
 * Configure a single server quota.
 */
static void
configure_server_quota(const cfg_obj_t **maps, const char *name,
		       isc_quota_t *quota)
{
	const cfg_obj_t *obj = NULL;
	isc_result_t result;

	result = ns_config_get(maps, name, &obj);
	INSIST(result == ISC_R_SUCCESS);
	isc_quota_max(quota, cfg_obj_asuint32(obj));
}

/*
 * This function is called as soon as the 'directory' statement has been
 * parsed.  This can be extended to support other options if necessary.
 */
static isc_result_t
directory_callback(const char *clausename, const cfg_obj_t *obj, void *arg) {
	isc_result_t result;
	const char *directory;

	REQUIRE(strcasecmp("directory", clausename) == 0);

	UNUSED(arg);
	UNUSED(clausename);

	/*
	 * Change directory.
	 */
	directory = cfg_obj_asstring(obj);

	if (! isc_file_ischdiridempotent(directory))
		cfg_obj_log(obj, ns_g_lctx, ISC_LOG_WARNING,
			    "option 'directory' contains relative path '%s'",
			    directory);

	result = isc_dir_chdir(directory);
	if (result != ISC_R_SUCCESS) {
		cfg_obj_log(obj, ns_g_lctx, ISC_LOG_ERROR,
			    "change directory to '%s' failed: %s",
			    directory, isc_result_totext(result));
		return (result);
	}

	return (ISC_R_SUCCESS);
}

static void
scan_interfaces(ns_server_t *server, isc_boolean_t verbose) {
	isc_boolean_t match_mapped = server->aclenv.match_mapped;

	ns_interfacemgr_scan(server->interfacemgr, verbose);
	/*
	 * Update the "localhost" and "localnets" ACLs to match the
	 * current set of network interfaces.
	 */
	dns_aclenv_copy(&server->aclenv,
			ns_interfacemgr_getaclenv(server->interfacemgr));

	server->aclenv.match_mapped = match_mapped;
}

static isc_result_t
add_listenelt(isc_mem_t *mctx, ns_listenlist_t *list, isc_sockaddr_t *addr,
	      isc_boolean_t wcardport_ok)
{
	ns_listenelt_t *lelt = NULL;
	dns_acl_t *src_acl = NULL;
	isc_result_t result;
	isc_sockaddr_t any_sa6;
	isc_netaddr_t netaddr;

	REQUIRE(isc_sockaddr_pf(addr) == AF_INET6);

	isc_sockaddr_any6(&any_sa6);
	if (!isc_sockaddr_equal(&any_sa6, addr) &&
	    (wcardport_ok || isc_sockaddr_getport(addr) != 0)) {
		isc_netaddr_fromin6(&netaddr, &addr->type.sin6.sin6_addr);

		result = dns_acl_create(mctx, 0, &src_acl);
		if (result != ISC_R_SUCCESS)
			return (result);

		result = dns_iptable_addprefix(src_acl->iptable,
					       &netaddr, 128, ISC_TRUE);
		if (result != ISC_R_SUCCESS)
			goto clean;

		result = ns_listenelt_create(mctx, isc_sockaddr_getport(addr),
					     src_acl, &lelt);
		if (result != ISC_R_SUCCESS)
			goto clean;
		ISC_LIST_APPEND(list->elts, lelt, link);
	}

	return (ISC_R_SUCCESS);

 clean:
	INSIST(lelt == NULL);
	dns_acl_detach(&src_acl);

	return (result);
}

/*
 * Make a list of xxx-source addresses and call ns_interfacemgr_adjust()
 * to update the listening interfaces accordingly.
 * We currently only consider IPv6, because this only affects IPv6 wildcard
 * sockets.
 */
static void
adjust_interfaces(ns_server_t *server, isc_mem_t *mctx) {
	isc_result_t result;
	ns_listenlist_t *list = NULL;
	dns_view_t *view;
	dns_zone_t *zone, *next;
	isc_sockaddr_t addr, *addrp;

	result = ns_listenlist_create(mctx, &list);
	if (result != ISC_R_SUCCESS)
		return;

	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link)) {
		dns_dispatch_t *dispatch6;

		dispatch6 = dns_resolver_dispatchv6(view->resolver);
		if (dispatch6 == NULL)
			continue;
		result = dns_dispatch_getlocaladdress(dispatch6, &addr);
		if (result != ISC_R_SUCCESS)
			goto fail;

		/*
		 * We always add non-wildcard address regardless of whether
		 * the port is 'any' (the fourth arg is TRUE): if the port is
		 * specific, we need to add it since it may conflict with a
		 * listening interface; if it's zero, we'll dynamically open
		 * query ports, and some of them may override an existing
		 * wildcard IPv6 port.
		 */
		result = add_listenelt(mctx, list, &addr, ISC_TRUE);
		if (result != ISC_R_SUCCESS)
			goto fail;
	}

	zone = NULL;
	for (result = dns_zone_first(server->zonemgr, &zone);
	     result == ISC_R_SUCCESS;
	     next = NULL, result = dns_zone_next(zone, &next), zone = next) {
		dns_view_t *zoneview;

		/*
		 * At this point the zone list may contain a stale zone
		 * just removed from the configuration.  To see the validity,
		 * check if the corresponding view is in our current view list.
		 * There may also be old zones that are still in the process
		 * of shutting down and have detached from their old view
		 * (zoneview == NULL).
		 */
		zoneview = dns_zone_getview(zone);
		if (zoneview == NULL)
			continue;
		for (view = ISC_LIST_HEAD(server->viewlist);
		     view != NULL && view != zoneview;
		     view = ISC_LIST_NEXT(view, link))
			;
		if (view == NULL)
			continue;

		addrp = dns_zone_getnotifysrc6(zone);
		result = add_listenelt(mctx, list, addrp, ISC_FALSE);
		if (result != ISC_R_SUCCESS)
			goto fail;

		addrp = dns_zone_getxfrsource6(zone);
		result = add_listenelt(mctx, list, addrp, ISC_FALSE);
		if (result != ISC_R_SUCCESS)
			goto fail;
	}

	ns_interfacemgr_adjust(server->interfacemgr, list, ISC_TRUE);

 clean:
	ns_listenlist_detach(&list);
	return;

 fail:
	/*
	 * Even when we failed the procedure, most of other interfaces
	 * should work correctly.  We therefore just warn it.
	 */
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_SERVER, ISC_LOG_WARNING,
		      "could not adjust the listen-on list; "
		      "some interfaces may not work");
	goto clean;
}

/*
 * This event callback is invoked to do periodic network
 * interface scanning.
 */
static void
interface_timer_tick(isc_task_t *task, isc_event_t *event) {
	isc_result_t result;
	ns_server_t *server = (ns_server_t *) event->ev_arg;
	INSIST(task == server->task);
	UNUSED(task);
	isc_event_free(&event);
	/*
	 * XXX should scan interfaces unlocked and get exclusive access
	 * only to replace ACLs.
	 */
	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	scan_interfaces(server, ISC_FALSE);
	isc_task_endexclusive(server->task);
}

static void
heartbeat_timer_tick(isc_task_t *task, isc_event_t *event) {
	ns_server_t *server = (ns_server_t *) event->ev_arg;
	dns_view_t *view;

	UNUSED(task);
	isc_event_free(&event);
	view = ISC_LIST_HEAD(server->viewlist);
	while (view != NULL) {
		dns_view_dialup(view);
		view = ISC_LIST_NEXT(view, link);
	}
}

static void
pps_timer_tick(isc_task_t *task, isc_event_t *event) {
	static unsigned int oldrequests = 0;
	unsigned int requests = ns_client_requests;

	UNUSED(task);
	isc_event_free(&event);

	/*
	 * Don't worry about wrapping as the overflow result will be right.
	 */
	dns_pps = (requests - oldrequests) / 1200;
	oldrequests = requests;
}

/*
 * Replace the current value of '*field', a dynamically allocated
 * string or NULL, with a dynamically allocated copy of the
 * null-terminated string pointed to by 'value', or NULL.
 */
static isc_result_t
setstring(ns_server_t *server, char **field, const char *value) {
	char *copy;

	if (value != NULL) {
		copy = isc_mem_strdup(server->mctx, value);
		if (copy == NULL)
			return (ISC_R_NOMEMORY);
	} else {
		copy = NULL;
	}

	if (*field != NULL)
		isc_mem_free(server->mctx, *field);

	*field = copy;
	return (ISC_R_SUCCESS);
}

/*
 * Replace the current value of '*field', a dynamically allocated
 * string or NULL, with another dynamically allocated string
 * or NULL if whether 'obj' is a string or void value, respectively.
 */
static isc_result_t
setoptstring(ns_server_t *server, char **field, const cfg_obj_t *obj) {
	if (cfg_obj_isvoid(obj))
		return (setstring(server, field, NULL));
	else
		return (setstring(server, field, cfg_obj_asstring(obj)));
}

static void
set_limit(const cfg_obj_t **maps, const char *configname,
	  const char *description, isc_resource_t resourceid,
	  isc_resourcevalue_t defaultvalue)
{
	const cfg_obj_t *obj = NULL;
	const char *resource;
	isc_resourcevalue_t value;
	isc_result_t result;

	if (ns_config_get(maps, configname, &obj) != ISC_R_SUCCESS)
		return;

	if (cfg_obj_isstring(obj)) {
		resource = cfg_obj_asstring(obj);
		if (strcasecmp(resource, "unlimited") == 0)
			value = ISC_RESOURCE_UNLIMITED;
		else {
			INSIST(strcasecmp(resource, "default") == 0);
			value = defaultvalue;
		}
	} else
		value = cfg_obj_asuint64(obj);

	result = isc_resource_setlimit(resourceid, value);
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      result == ISC_R_SUCCESS ?
			ISC_LOG_DEBUG(3) : ISC_LOG_WARNING,
		      "set maximum %s to %" ISC_PRINT_QUADFORMAT "u: %s",
		      description, value, isc_result_totext(result));
}

#define SETLIMIT(cfgvar, resource, description) \
	set_limit(maps, cfgvar, description, isc_resource_ ## resource, \
		  ns_g_init ## resource)

static void
set_limits(const cfg_obj_t **maps) {
	SETLIMIT("stacksize", stacksize, "stack size");
	SETLIMIT("datasize", datasize, "data size");
	SETLIMIT("coresize", coresize, "core size");
	SETLIMIT("files", openfiles, "open files");
}

static void
portset_fromconf(isc_portset_t *portset, const cfg_obj_t *ports,
		 isc_boolean_t positive)
{
	const cfg_listelt_t *element;

	for (element = cfg_list_first(ports);
	     element != NULL;
	     element = cfg_list_next(element)) {
		const cfg_obj_t *obj = cfg_listelt_value(element);

		if (cfg_obj_isuint32(obj)) {
			in_port_t port = (in_port_t)cfg_obj_asuint32(obj);

			if (positive)
				isc_portset_add(portset, port);
			else
				isc_portset_remove(portset, port);
		} else {
			const cfg_obj_t *obj_loport, *obj_hiport;
			in_port_t loport, hiport;

			obj_loport = cfg_tuple_get(obj, "loport");
			loport = (in_port_t)cfg_obj_asuint32(obj_loport);
			obj_hiport = cfg_tuple_get(obj, "hiport");
			hiport = (in_port_t)cfg_obj_asuint32(obj_hiport);

			if (positive)
				isc_portset_addrange(portset, loport, hiport);
			else {
				isc_portset_removerange(portset, loport,
							hiport);
			}
		}
	}
}

static isc_result_t
removed(dns_zone_t *zone, void *uap) {
	const char *type;

	if (dns_zone_getview(zone) != uap)
		return (ISC_R_SUCCESS);

	switch (dns_zone_gettype(zone)) {
	case dns_zone_master:
		type = "master";
		break;
	case dns_zone_slave:
		type = "slave";
		break;
	case dns_zone_stub:
		type = "stub";
		break;
	default:
		type = "other";
		break;
	}
	dns_zone_log(zone, ISC_LOG_INFO, "(%s) removed", type);
	return (ISC_R_SUCCESS);
}

static isc_result_t
load_configuration(const char *filename, ns_server_t *server,
		   isc_boolean_t first_time)
{
	cfg_aclconfctx_t aclconfctx;
	cfg_obj_t *config;
	cfg_parser_t *parser = NULL;
	const cfg_listelt_t *element;
	const cfg_obj_t *builtin_views;
	const cfg_obj_t *maps[3];
	const cfg_obj_t *obj;
	const cfg_obj_t *options;
	const cfg_obj_t *usev4ports, *avoidv4ports, *usev6ports, *avoidv6ports;
	const cfg_obj_t *views;
	dns_view_t *view = NULL;
	dns_view_t *view_next;
	dns_viewlist_t tmpviewlist;
	dns_viewlist_t viewlist;
	in_port_t listen_port, udpport_low, udpport_high;
	int i;
	isc_interval_t interval;
	isc_portset_t *v4portset = NULL;
	isc_portset_t *v6portset = NULL;
	isc_resourcevalue_t nfiles;
	isc_result_t result;
	isc_uint32_t heartbeat_interval;
	isc_uint32_t interface_interval;
	isc_uint32_t reserved;
	isc_uint32_t udpsize;
	unsigned int maxsocks;

	cfg_aclconfctx_init(&aclconfctx);
	ISC_LIST_INIT(viewlist);

	/* Ensure exclusive access to configuration data. */
	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	/*
	 * Parse the global default pseudo-config file.
	 */
	if (first_time) {
		CHECK(ns_config_parsedefaults(ns_g_parser, &ns_g_config));
		RUNTIME_CHECK(cfg_map_get(ns_g_config, "options",
					  &ns_g_defaults) ==
			      ISC_R_SUCCESS);
	}

	/*
	 * Parse the configuration file using the new config code.
	 */
	result = ISC_R_FAILURE;
	config = NULL;

	/*
	 * Unless this is lwresd with the -C option, parse the config file.
	 */
	if (!(ns_g_lwresdonly && lwresd_g_useresolvconf)) {
		isc_log_write(ns_g_lctx,
			      NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
			      ISC_LOG_INFO, "loading configuration from '%s'",
			      filename);
		CHECK(cfg_parser_create(ns_g_mctx, ns_g_lctx, &parser));
		cfg_parser_setcallback(parser, directory_callback, NULL);
		result = cfg_parse_file(parser, filename, &cfg_type_namedconf,
					&config);
	}

	/*
	 * If this is lwresd with the -C option, or lwresd with no -C or -c
	 * option where the above parsing failed, parse resolv.conf.
	 */
	if (ns_g_lwresdonly &&
	    (lwresd_g_useresolvconf ||
	     (!ns_g_conffileset && result == ISC_R_FILENOTFOUND)))
	{
		isc_log_write(ns_g_lctx,
			      NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
			      ISC_LOG_INFO, "loading configuration from '%s'",
			      lwresd_g_resolvconffile);
		if (parser != NULL)
			cfg_parser_destroy(&parser);
		CHECK(cfg_parser_create(ns_g_mctx, ns_g_lctx, &parser));
		result = ns_lwresd_parseeresolvconf(ns_g_mctx, parser,
						    &config);
	}
	CHECK(result);

	/*
	 * Check the validity of the configuration.
	 */
	CHECK(bind9_check_namedconf(config, ns_g_lctx, ns_g_mctx));

	/*
	 * Fill in the maps array, used for resolving defaults.
	 */
	i = 0;
	options = NULL;
	result = cfg_map_get(config, "options", &options);
	if (result == ISC_R_SUCCESS)
		maps[i++] = options;
	maps[i++] = ns_g_defaults;
	maps[i++] = NULL;

	/*
	 * Set process limits, which (usually) needs to be done as root.
	 */
	set_limits(maps);

	/*
	 * Check if max number of open sockets that the system allows is
	 * sufficiently large.  Failing this condition is not necessarily fatal,
	 * but may cause subsequent runtime failures for a busy recursive
	 * server.
	 */
	result = isc_socketmgr_getmaxsockets(ns_g_socketmgr, &maxsocks);
	if (result != ISC_R_SUCCESS)
		maxsocks = 0;
	result = isc_resource_getcurlimit(isc_resource_openfiles, &nfiles);
	if (result == ISC_R_SUCCESS && (isc_resourcevalue_t)maxsocks > nfiles) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_WARNING,
			      "max open files (%" ISC_PRINT_QUADFORMAT "u)"
			      " is smaller than max sockets (%u)",
			      nfiles, maxsocks);
	}

	/*
	 * Set the number of socket reserved for TCP, stdio etc.
	 */
	obj = NULL;
	result = ns_config_get(maps, "reserved-sockets", &obj);
	INSIST(result == ISC_R_SUCCESS);
	reserved = cfg_obj_asuint32(obj);
	if (maxsocks != 0) {
		if (maxsocks < 128U)			/* Prevent underflow. */
			reserved = 0;
		else if (reserved > maxsocks - 128U)	/* Minimum UDP space. */
			reserved = maxsocks - 128;
	}
	/* Minimum TCP/stdio space. */
	if (reserved < 128U)
		reserved = 128;
	if (reserved + 128U > maxsocks && maxsocks != 0) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_WARNING,
			      "less than 128 UDP sockets available after "
			      "applying 'reserved-sockets' and 'maxsockets'");
	}
	isc__socketmgr_setreserved(ns_g_socketmgr, reserved);

	/*
	 * Configure various server options.
	 */
	configure_server_quota(maps, "transfers-out", &server->xfroutquota);
	configure_server_quota(maps, "tcp-clients", &server->tcpquota);
	configure_server_quota(maps, "recursive-clients",
			       &server->recursionquota);
	if (server->recursionquota.max > 1000)
		isc_quota_soft(&server->recursionquota,
			       server->recursionquota.max - 100);
	else
		isc_quota_soft(&server->recursionquota, 0);

	CHECK(configure_view_acl(NULL, config, "blackhole", &aclconfctx,
				 ns_g_mctx, &server->blackholeacl));
	if (server->blackholeacl != NULL)
		dns_dispatchmgr_setblackhole(ns_g_dispatchmgr,
					     server->blackholeacl);

	obj = NULL;
	result = ns_config_get(maps, "match-mapped-addresses", &obj);
	INSIST(result == ISC_R_SUCCESS);
	server->aclenv.match_mapped = cfg_obj_asboolean(obj);

	CHECKM(ns_statschannels_configure(ns_g_server, config, &aclconfctx),
	       "configuring statistics server(s)");

	/*
	 * Configure sets of UDP query source ports.
	 */
	CHECKM(isc_portset_create(ns_g_mctx, &v4portset),
	       "creating UDP port set");
	CHECKM(isc_portset_create(ns_g_mctx, &v6portset),
	       "creating UDP port set");

	usev4ports = NULL;
	usev6ports = NULL;
	avoidv4ports = NULL;
	avoidv6ports = NULL;

	(void)ns_config_get(maps, "use-v4-udp-ports", &usev4ports);
	if (usev4ports != NULL)
		portset_fromconf(v4portset, usev4ports, ISC_TRUE);
	else {
		CHECKM(isc_net_getudpportrange(AF_INET, &udpport_low,
					       &udpport_high),
		       "get the default UDP/IPv4 port range");
		if (udpport_low == udpport_high)
			isc_portset_add(v4portset, udpport_low);
		else {
			isc_portset_addrange(v4portset, udpport_low,
					     udpport_high);
		}
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
			      "using default UDP/IPv4 port range: [%d, %d]",
			      udpport_low, udpport_high);
	}
	(void)ns_config_get(maps, "avoid-v4-udp-ports", &avoidv4ports);
	if (avoidv4ports != NULL)
		portset_fromconf(v4portset, avoidv4ports, ISC_FALSE);

	(void)ns_config_get(maps, "use-v6-udp-ports", &usev6ports);
	if (usev6ports != NULL)
		portset_fromconf(v6portset, usev6ports, ISC_TRUE);
	else {
		CHECKM(isc_net_getudpportrange(AF_INET6, &udpport_low,
					       &udpport_high),
		       "get the default UDP/IPv6 port range");
		if (udpport_low == udpport_high)
			isc_portset_add(v6portset, udpport_low);
		else {
			isc_portset_addrange(v6portset, udpport_low,
					     udpport_high);
		}
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
			      "using default UDP/IPv6 port range: [%d, %d]",
			      udpport_low, udpport_high);
	}
	(void)ns_config_get(maps, "avoid-v6-udp-ports", &avoidv6ports);
	if (avoidv6ports != NULL)
		portset_fromconf(v6portset, avoidv6ports, ISC_FALSE);

	dns_dispatchmgr_setavailports(ns_g_dispatchmgr, v4portset, v6portset);

	/*
	 * Set the EDNS UDP size when we don't match a view.
	 */
	obj = NULL;
	result = ns_config_get(maps, "edns-udp-size", &obj);
	INSIST(result == ISC_R_SUCCESS);
	udpsize = cfg_obj_asuint32(obj);
	if (udpsize < 512)
		udpsize = 512;
	if (udpsize > 4096)
		udpsize = 4096;
	ns_g_udpsize = (isc_uint16_t)udpsize;

	/*
	 * Configure the zone manager.
	 */
	obj = NULL;
	result = ns_config_get(maps, "transfers-in", &obj);
	INSIST(result == ISC_R_SUCCESS);
	dns_zonemgr_settransfersin(server->zonemgr, cfg_obj_asuint32(obj));

	obj = NULL;
	result = ns_config_get(maps, "transfers-per-ns", &obj);
	INSIST(result == ISC_R_SUCCESS);
	dns_zonemgr_settransfersperns(server->zonemgr, cfg_obj_asuint32(obj));

	obj = NULL;
	result = ns_config_get(maps, "serial-query-rate", &obj);
	INSIST(result == ISC_R_SUCCESS);
	dns_zonemgr_setserialqueryrate(server->zonemgr, cfg_obj_asuint32(obj));

	/*
	 * Determine which port to use for listening for incoming connections.
	 */
	if (ns_g_port != 0)
		listen_port = ns_g_port;
	else
		CHECKM(ns_config_getport(config, &listen_port), "port");

	/*
	 * Find the listen queue depth.
	 */
	obj = NULL;
	result = ns_config_get(maps, "tcp-listen-queue", &obj);
	INSIST(result == ISC_R_SUCCESS);
	ns_g_listen = cfg_obj_asuint32(obj);
	if (ns_g_listen < 3)
		ns_g_listen = 3;

	/*
	 * Configure the interface manager according to the "listen-on"
	 * statement.
	 */
	{
		const cfg_obj_t *clistenon = NULL;
		ns_listenlist_t *listenon = NULL;

		clistenon = NULL;
		/*
		 * Even though listen-on is present in the default
		 * configuration, we can't use it here, since it isn't
		 * used if we're in lwresd mode.  This way is easier.
		 */
		if (options != NULL)
			(void)cfg_map_get(options, "listen-on", &clistenon);
		if (clistenon != NULL) {
			result = ns_listenlist_fromconfig(clistenon,
							  config,
							  &aclconfctx,
							  ns_g_mctx,
							  &listenon);
		} else if (!ns_g_lwresdonly) {
			/*
			 * Not specified, use default.
			 */
			CHECK(ns_listenlist_default(ns_g_mctx, listen_port,
						    ISC_TRUE, &listenon));
		}
		if (listenon != NULL) {
			ns_interfacemgr_setlistenon4(server->interfacemgr,
						     listenon);
			ns_listenlist_detach(&listenon);
		}
	}
	/*
	 * Ditto for IPv6.
	 */
	{
		const cfg_obj_t *clistenon = NULL;
		ns_listenlist_t *listenon = NULL;

		if (options != NULL)
			(void)cfg_map_get(options, "listen-on-v6", &clistenon);
		if (clistenon != NULL) {
			result = ns_listenlist_fromconfig(clistenon,
							  config,
							  &aclconfctx,
							  ns_g_mctx,
							  &listenon);
		} else if (!ns_g_lwresdonly) {
			isc_boolean_t enable;
			/*
			 * Not specified, use default.
			 */
			enable = ISC_TF(isc_net_probeipv4() != ISC_R_SUCCESS);
			CHECK(ns_listenlist_default(ns_g_mctx, listen_port,
						    enable, &listenon));
		}
		if (listenon != NULL) {
			ns_interfacemgr_setlistenon6(server->interfacemgr,
						     listenon);
			ns_listenlist_detach(&listenon);
		}
	}

	/*
	 * Rescan the interface list to pick up changes in the
	 * listen-on option.  It's important that we do this before we try
	 * to configure the query source, since the dispatcher we use might
	 * be shared with an interface.
	 */
	scan_interfaces(server, ISC_TRUE);

	/*
	 * Arrange for further interface scanning to occur periodically
	 * as specified by the "interface-interval" option.
	 */
	obj = NULL;
	result = ns_config_get(maps, "interface-interval", &obj);
	INSIST(result == ISC_R_SUCCESS);
	interface_interval = cfg_obj_asuint32(obj) * 60;
	if (interface_interval == 0) {
		CHECK(isc_timer_reset(server->interface_timer,
				      isc_timertype_inactive,
				      NULL, NULL, ISC_TRUE));
	} else if (server->interface_interval != interface_interval) {
		isc_interval_set(&interval, interface_interval, 0);
		CHECK(isc_timer_reset(server->interface_timer,
				      isc_timertype_ticker,
				      NULL, &interval, ISC_FALSE));
	}
	server->interface_interval = interface_interval;

	/*
	 * Configure the dialup heartbeat timer.
	 */
	obj = NULL;
	result = ns_config_get(maps, "heartbeat-interval", &obj);
	INSIST(result == ISC_R_SUCCESS);
	heartbeat_interval = cfg_obj_asuint32(obj) * 60;
	if (heartbeat_interval == 0) {
		CHECK(isc_timer_reset(server->heartbeat_timer,
				      isc_timertype_inactive,
				      NULL, NULL, ISC_TRUE));
	} else if (server->heartbeat_interval != heartbeat_interval) {
		isc_interval_set(&interval, heartbeat_interval, 0);
		CHECK(isc_timer_reset(server->heartbeat_timer,
				      isc_timertype_ticker,
				      NULL, &interval, ISC_FALSE));
	}
	server->heartbeat_interval = heartbeat_interval;

	isc_interval_set(&interval, 1200, 0);
	CHECK(isc_timer_reset(server->pps_timer, isc_timertype_ticker, NULL,
			      &interval, ISC_FALSE));

	/*
	 * Configure and freeze all explicit views.  Explicit
	 * views that have zones were already created at parsing
	 * time, but views with no zones must be created here.
	 */
	views = NULL;
	(void)cfg_map_get(config, "view", &views);
	for (element = cfg_list_first(views);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const cfg_obj_t *vconfig = cfg_listelt_value(element);
		view = NULL;

		CHECK(create_view(vconfig, &viewlist, &view));
		INSIST(view != NULL);
		CHECK(configure_view(view, config, vconfig,
				     ns_g_mctx, &aclconfctx, ISC_TRUE));
		dns_view_freeze(view);
		dns_view_detach(&view);
	}

	/*
	 * Make sure we have a default view if and only if there
	 * were no explicit views.
	 */
	if (views == NULL) {
		/*
		 * No explicit views; there ought to be a default view.
		 * There may already be one created as a side effect
		 * of zone statements, or we may have to create one.
		 * In either case, we need to configure and freeze it.
		 */
		CHECK(create_view(NULL, &viewlist, &view));
		CHECK(configure_view(view, config, NULL, ns_g_mctx,
				     &aclconfctx, ISC_TRUE));
		dns_view_freeze(view);
		dns_view_detach(&view);
	}

	/*
	 * Create (or recreate) the built-in views.  Currently
	 * there is only one, the _bind view.
	 */
	builtin_views = NULL;
	RUNTIME_CHECK(cfg_map_get(ns_g_config, "view",
				  &builtin_views) == ISC_R_SUCCESS);
	for (element = cfg_list_first(builtin_views);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const cfg_obj_t *vconfig = cfg_listelt_value(element);
		CHECK(create_view(vconfig, &viewlist, &view));
		CHECK(configure_view(view, config, vconfig, ns_g_mctx,
				     &aclconfctx, ISC_FALSE));
		dns_view_freeze(view);
		dns_view_detach(&view);
		view = NULL;
	}

	/*
	 * Swap our new view list with the production one.
	 */
	tmpviewlist = server->viewlist;
	server->viewlist = viewlist;
	viewlist = tmpviewlist;

	/*
	 * Load the TKEY information from the configuration.
	 */
	if (options != NULL) {
		dns_tkeyctx_t *t = NULL;
		CHECKM(ns_tkeyctx_fromconfig(options, ns_g_mctx, ns_g_entropy,
					     &t),
		       "configuring TKEY");
		if (server->tkeyctx != NULL)
			dns_tkeyctx_destroy(&server->tkeyctx);
		server->tkeyctx = t;
	}

	/*
	 * Bind the control port(s).
	 */
	CHECKM(ns_controls_configure(ns_g_server->controls, config,
				     &aclconfctx),
	       "binding control channel(s)");

	/*
	 * Bind the lwresd port(s).
	 */
	CHECKM(ns_lwresd_configure(ns_g_mctx, config),
	       "binding lightweight resolver ports");

	/*
	 * Open the source of entropy.
	 */
	if (first_time) {
		obj = NULL;
		result = ns_config_get(maps, "random-device", &obj);
		if (result != ISC_R_SUCCESS) {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
				      "no source of entropy found");
		} else {
			const char *randomdev = cfg_obj_asstring(obj);
			result = isc_entropy_createfilesource(ns_g_entropy,
							      randomdev);
			if (result != ISC_R_SUCCESS)
				isc_log_write(ns_g_lctx,
					      NS_LOGCATEGORY_GENERAL,
					      NS_LOGMODULE_SERVER,
					      ISC_LOG_INFO,
					      "could not open entropy source "
					      "%s: %s",
					      randomdev,
					      isc_result_totext(result));
#ifdef PATH_RANDOMDEV
			if (ns_g_fallbackentropy != NULL) {
				if (result != ISC_R_SUCCESS) {
					isc_log_write(ns_g_lctx,
						      NS_LOGCATEGORY_GENERAL,
						      NS_LOGMODULE_SERVER,
						      ISC_LOG_INFO,
						      "using pre-chroot entropy source "
						      "%s",
						      PATH_RANDOMDEV);
					isc_entropy_detach(&ns_g_entropy);
					isc_entropy_attach(ns_g_fallbackentropy,
							   &ns_g_entropy);
				}
				isc_entropy_detach(&ns_g_fallbackentropy);
			}
#endif
		}
	}

	/*
	 * Relinquish root privileges.
	 */
	if (first_time)
		ns_os_changeuser();

	/*
	 * Check that the working directory is writable.
	 */
	if (access(".", W_OK) != 0) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "the working directory is not writable");
	}

	/*
	 * Configure the logging system.
	 *
	 * Do this after changing UID to make sure that any log
	 * files specified in named.conf get created by the
	 * unprivileged user, not root.
	 */
	if (ns_g_logstderr) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
			      "ignoring config file logging "
			      "statement due to -g option");
	} else {
		const cfg_obj_t *logobj = NULL;
		isc_logconfig_t *logc = NULL;

		CHECKM(isc_logconfig_create(ns_g_lctx, &logc),
		       "creating new logging configuration");

		logobj = NULL;
		(void)cfg_map_get(config, "logging", &logobj);
		if (logobj != NULL) {
			CHECKM(ns_log_configure(logc, logobj),
			       "configuring logging");
		} else {
			CHECKM(ns_log_setdefaultchannels(logc),
			       "setting up default logging channels");
			CHECKM(ns_log_setunmatchedcategory(logc),
			       "setting up default 'category unmatched'");
			CHECKM(ns_log_setdefaultcategory(logc),
			       "setting up default 'category default'");
		}

		result = isc_logconfig_use(ns_g_lctx, logc);
		if (result != ISC_R_SUCCESS) {
			isc_logconfig_destroy(&logc);
			CHECKM(result, "installing logging configuration");
		}

		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_DEBUG(1),
			      "now using logging configuration from "
			      "config file");
	}

	/*
	 * Set the default value of the query logging flag depending
	 * whether a "queries" category has been defined.  This is
	 * a disgusting hack, but we need to do this for BIND 8
	 * compatibility.
	 */
	if (first_time) {
		const cfg_obj_t *logobj = NULL;
		const cfg_obj_t *categories = NULL;

		obj = NULL;
		if (ns_config_get(maps, "querylog", &obj) == ISC_R_SUCCESS) {
			server->log_queries = cfg_obj_asboolean(obj);
		} else {

			(void)cfg_map_get(config, "logging", &logobj);
			if (logobj != NULL)
				(void)cfg_map_get(logobj, "category",
						  &categories);
			if (categories != NULL) {
				const cfg_listelt_t *element;
				for (element = cfg_list_first(categories);
				     element != NULL;
				     element = cfg_list_next(element))
				{
					const cfg_obj_t *catobj;
					const char *str;

					obj = cfg_listelt_value(element);
					catobj = cfg_tuple_get(obj, "name");
					str = cfg_obj_asstring(catobj);
					if (strcasecmp(str, "queries") == 0)
						server->log_queries = ISC_TRUE;
				}
			}
		}
	}

	obj = NULL;
	if (ns_config_get(maps, "pid-file", &obj) == ISC_R_SUCCESS)
		if (cfg_obj_isvoid(obj))
			ns_os_writepidfile(NULL, first_time);
		else
			ns_os_writepidfile(cfg_obj_asstring(obj), first_time);
	else if (ns_g_lwresdonly)
		ns_os_writepidfile(lwresd_g_defaultpidfile, first_time);
	else
		ns_os_writepidfile(ns_g_defaultpidfile, first_time);

	obj = NULL;
	if (options != NULL &&
	    cfg_map_get(options, "memstatistics", &obj) == ISC_R_SUCCESS)
		ns_g_memstatistics = cfg_obj_asboolean(obj);
	else
		ns_g_memstatistics =
			ISC_TF((isc_mem_debugging & ISC_MEM_DEBUGRECORD) != 0);

	obj = NULL;
	if (ns_config_get(maps, "memstatistics-file", &obj) == ISC_R_SUCCESS)
		ns_main_setmemstats(cfg_obj_asstring(obj));
	else if (ns_g_memstatistics)
		ns_main_setmemstats("named.memstats");
	else
		ns_main_setmemstats(NULL);

	obj = NULL;
	result = ns_config_get(maps, "statistics-file", &obj);
	INSIST(result == ISC_R_SUCCESS);
	CHECKM(setstring(server, &server->statsfile, cfg_obj_asstring(obj)),
	       "strdup");

	obj = NULL;
	result = ns_config_get(maps, "dump-file", &obj);
	INSIST(result == ISC_R_SUCCESS);
	CHECKM(setstring(server, &server->dumpfile, cfg_obj_asstring(obj)),
	       "strdup");

	obj = NULL;
	result = ns_config_get(maps, "recursing-file", &obj);
	INSIST(result == ISC_R_SUCCESS);
	CHECKM(setstring(server, &server->recfile, cfg_obj_asstring(obj)),
	       "strdup");

	obj = NULL;
	result = ns_config_get(maps, "version", &obj);
	if (result == ISC_R_SUCCESS) {
		CHECKM(setoptstring(server, &server->version, obj), "strdup");
		server->version_set = ISC_TRUE;
	} else {
		server->version_set = ISC_FALSE;
	}

	obj = NULL;
	result = ns_config_get(maps, "hostname", &obj);
	if (result == ISC_R_SUCCESS) {
		CHECKM(setoptstring(server, &server->hostname, obj), "strdup");
		server->hostname_set = ISC_TRUE;
	} else {
		server->hostname_set = ISC_FALSE;
	}

	obj = NULL;
	result = ns_config_get(maps, "server-id", &obj);
	server->server_usehostname = ISC_FALSE;
	if (result == ISC_R_SUCCESS && cfg_obj_isboolean(obj)) {
		/* The parser translates "hostname" to ISC_TRUE */
		server->server_usehostname = cfg_obj_asboolean(obj);
		result = setstring(server, &server->server_id, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
	} else if (result == ISC_R_SUCCESS) {
		/* Found a quoted string */
		CHECKM(setoptstring(server, &server->server_id, obj), "strdup");
	} else {
		result = setstring(server, &server->server_id, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
	}

	obj = NULL;
	result = ns_config_get(maps, "flush-zones-on-shutdown", &obj);
	if (result == ISC_R_SUCCESS) {
		server->flushonshutdown = cfg_obj_asboolean(obj);
	} else {
		server->flushonshutdown = ISC_FALSE;
	}

	result = ISC_R_SUCCESS;

 cleanup:
	if (v4portset != NULL)
		isc_portset_destroy(ns_g_mctx, &v4portset);

	if (v6portset != NULL)
		isc_portset_destroy(ns_g_mctx, &v6portset);

	cfg_aclconfctx_destroy(&aclconfctx);

	if (parser != NULL) {
		if (config != NULL)
			cfg_obj_destroy(parser, &config);
		cfg_parser_destroy(&parser);
	}

	if (view != NULL)
		dns_view_detach(&view);

	/*
	 * This cleans up either the old production view list
	 * or our temporary list depending on whether they
	 * were swapped above or not.
	 */
	for (view = ISC_LIST_HEAD(viewlist);
	     view != NULL;
	     view = view_next) {
		view_next = ISC_LIST_NEXT(view, link);
		ISC_LIST_UNLINK(viewlist, view, link);
		if (result == ISC_R_SUCCESS &&
		    strcmp(view->name, "_bind") != 0)
			(void)dns_zt_apply(view->zonetable, ISC_FALSE,
					   removed, view);
		dns_view_detach(&view);
	}

	/*
	 * Adjust the listening interfaces in accordance with the source
	 * addresses specified in views and zones.
	 */
	if (isc_net_probeipv6() == ISC_R_SUCCESS)
		adjust_interfaces(server, ns_g_mctx);

	/* Relinquish exclusive access to configuration data. */
	isc_task_endexclusive(server->task);

	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      ISC_LOG_DEBUG(1), "load_configuration: %s",
		      isc_result_totext(result));

	return (result);
}

static isc_result_t
load_zones(ns_server_t *server, isc_boolean_t stop) {
	isc_result_t result;
	dns_view_t *view;

	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	/*
	 * Load zone data from disk.
	 */
	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link))
	{
		CHECK(dns_view_load(view, stop));
	}

	/*
	 * Force zone maintenance.  Do this after loading
	 * so that we know when we need to force AXFR of
	 * slave zones whose master files are missing.
	 */
	CHECK(dns_zonemgr_forcemaint(server->zonemgr));
 cleanup:
	isc_task_endexclusive(server->task);
	return (result);
}

static isc_result_t
load_new_zones(ns_server_t *server, isc_boolean_t stop) {
	isc_result_t result;
	dns_view_t *view;

	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	/*
	 * Load zone data from disk.
	 */
	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link))
	{
		CHECK(dns_view_loadnew(view, stop));
	}
	/*
	 * Force zone maintenance.  Do this after loading
	 * so that we know when we need to force AXFR of
	 * slave zones whose master files are missing.
	 */
	dns_zonemgr_resumexfrs(server->zonemgr);
 cleanup:
	isc_task_endexclusive(server->task);
	return (result);
}

static void
run_server(isc_task_t *task, isc_event_t *event) {
	isc_result_t result;
	ns_server_t *server = (ns_server_t *)event->ev_arg;

	INSIST(task == server->task);

	isc_event_free(&event);

	CHECKFATAL(dns_dispatchmgr_create(ns_g_mctx, ns_g_entropy,
					  &ns_g_dispatchmgr),
		   "creating dispatch manager");

	dns_dispatchmgr_setstats(ns_g_dispatchmgr, server->resolverstats);

	CHECKFATAL(ns_interfacemgr_create(ns_g_mctx, ns_g_taskmgr,
					  ns_g_socketmgr, ns_g_dispatchmgr,
					  &server->interfacemgr),
		   "creating interface manager");

	CHECKFATAL(isc_timer_create(ns_g_timermgr, isc_timertype_inactive,
				    NULL, NULL, server->task,
				    interface_timer_tick,
				    server, &server->interface_timer),
		   "creating interface timer");

	CHECKFATAL(isc_timer_create(ns_g_timermgr, isc_timertype_inactive,
				    NULL, NULL, server->task,
				    heartbeat_timer_tick,
				    server, &server->heartbeat_timer),
		   "creating heartbeat timer");

	CHECKFATAL(isc_timer_create(ns_g_timermgr, isc_timertype_inactive,
				    NULL, NULL, server->task, pps_timer_tick,
				    server, &server->pps_timer),
		   "creating pps timer");

	CHECKFATAL(cfg_parser_create(ns_g_mctx, NULL, &ns_g_parser),
		   "creating default configuration parser");

	if (ns_g_lwresdonly)
		CHECKFATAL(load_configuration(lwresd_g_conffile, server,
					      ISC_TRUE),
			   "loading configuration");
	else
		CHECKFATAL(load_configuration(ns_g_conffile, server, ISC_TRUE),
			   "loading configuration");

	isc_hash_init();

	CHECKFATAL(load_zones(server, ISC_FALSE), "loading zones");

	ns_os_started();
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      ISC_LOG_NOTICE, "running");
}

void
ns_server_flushonshutdown(ns_server_t *server, isc_boolean_t flush) {

	REQUIRE(NS_SERVER_VALID(server));

	server->flushonshutdown = flush;
}

static void
shutdown_server(isc_task_t *task, isc_event_t *event) {
	isc_result_t result;
	dns_view_t *view, *view_next;
	ns_server_t *server = (ns_server_t *)event->ev_arg;
	isc_boolean_t flush = server->flushonshutdown;

	UNUSED(task);
	INSIST(task == server->task);

	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      ISC_LOG_INFO, "shutting down%s",
		      flush ? ": flushing changes" : "");

	ns_statschannels_shutdown(server);
	ns_controls_shutdown(server->controls);
	end_reserved_dispatches(server, ISC_TRUE);

	cfg_obj_destroy(ns_g_parser, &ns_g_config);
	cfg_parser_destroy(&ns_g_parser);

	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = view_next) {
		view_next = ISC_LIST_NEXT(view, link);
		ISC_LIST_UNLINK(server->viewlist, view, link);
		if (flush)
			dns_view_flushanddetach(&view);
		else
			dns_view_detach(&view);
	}

	isc_timer_detach(&server->interface_timer);
	isc_timer_detach(&server->heartbeat_timer);
	isc_timer_detach(&server->pps_timer);

	ns_interfacemgr_shutdown(server->interfacemgr);
	ns_interfacemgr_detach(&server->interfacemgr);

	dns_dispatchmgr_destroy(&ns_g_dispatchmgr);

	dns_zonemgr_shutdown(server->zonemgr);

	if (server->blackholeacl != NULL)
		dns_acl_detach(&server->blackholeacl);

	dns_db_detach(&server->in_roothints);

	isc_task_endexclusive(server->task);

	isc_task_detach(&server->task);

	isc_event_free(&event);
}

void
ns_server_create(isc_mem_t *mctx, ns_server_t **serverp) {
	isc_result_t result;

	ns_server_t *server = isc_mem_get(mctx, sizeof(*server));
	if (server == NULL)
		fatal("allocating server object", ISC_R_NOMEMORY);

	server->mctx = mctx;
	server->task = NULL;

	/* Initialize configuration data with default values. */

	result = isc_quota_init(&server->xfroutquota, 10);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	result = isc_quota_init(&server->tcpquota, 10);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	result = isc_quota_init(&server->recursionquota, 100);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	result = dns_aclenv_init(mctx, &server->aclenv);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	/* Initialize server data structures. */
	server->zonemgr = NULL;
	server->interfacemgr = NULL;
	ISC_LIST_INIT(server->viewlist);
	server->in_roothints = NULL;
	server->blackholeacl = NULL;

	CHECKFATAL(dns_rootns_create(mctx, dns_rdataclass_in, NULL,
				     &server->in_roothints),
		   "setting up root hints");

	CHECKFATAL(isc_mutex_init(&server->reload_event_lock),
		   "initializing reload event lock");
	server->reload_event =
		isc_event_allocate(ns_g_mctx, server,
				   NS_EVENT_RELOAD,
				   ns_server_reload,
				   server,
				   sizeof(isc_event_t));
	CHECKFATAL(server->reload_event == NULL ?
		   ISC_R_NOMEMORY : ISC_R_SUCCESS,
		   "allocating reload event");

	CHECKFATAL(dst_lib_init(ns_g_mctx, ns_g_entropy, ISC_ENTROPY_GOODONLY),
		   "initializing DST");

	server->tkeyctx = NULL;
	CHECKFATAL(dns_tkeyctx_create(ns_g_mctx, ns_g_entropy,
				      &server->tkeyctx),
		   "creating TKEY context");

	/*
	 * Setup the server task, which is responsible for coordinating
	 * startup and shutdown of the server.
	 */
	CHECKFATAL(isc_task_create(ns_g_taskmgr, 0, &server->task),
		   "creating server task");
	isc_task_setname(server->task, "server", server);
	CHECKFATAL(isc_task_onshutdown(server->task, shutdown_server, server),
		   "isc_task_onshutdown");
	CHECKFATAL(isc_app_onrun(ns_g_mctx, server->task, run_server, server),
		   "isc_app_onrun");

	server->interface_timer = NULL;
	server->heartbeat_timer = NULL;
	server->pps_timer = NULL;

	server->interface_interval = 0;
	server->heartbeat_interval = 0;

	CHECKFATAL(dns_zonemgr_create(ns_g_mctx, ns_g_taskmgr, ns_g_timermgr,
				      ns_g_socketmgr, &server->zonemgr),
		   "dns_zonemgr_create");

	server->statsfile = isc_mem_strdup(server->mctx, "named.stats");
	CHECKFATAL(server->statsfile == NULL ? ISC_R_NOMEMORY : ISC_R_SUCCESS,
		   "isc_mem_strdup");
	server->nsstats = NULL;
	server->rcvquerystats = NULL;
	server->opcodestats = NULL;
	server->zonestats = NULL;
	server->resolverstats = NULL;
	server->sockstats = NULL;
	CHECKFATAL(isc_stats_create(server->mctx, &server->sockstats,
				    isc_sockstatscounter_max),
		   "isc_stats_create");
	isc_socketmgr_setstats(ns_g_socketmgr, server->sockstats);

	server->dumpfile = isc_mem_strdup(server->mctx, "named_dump.db");
	CHECKFATAL(server->dumpfile == NULL ? ISC_R_NOMEMORY : ISC_R_SUCCESS,
		   "isc_mem_strdup");

	server->recfile = isc_mem_strdup(server->mctx, "named.recursing");
	CHECKFATAL(server->recfile == NULL ? ISC_R_NOMEMORY : ISC_R_SUCCESS,
		   "isc_mem_strdup");

	server->hostname_set = ISC_FALSE;
	server->hostname = NULL;
	server->version_set = ISC_FALSE;
	server->version = NULL;
	server->server_usehostname = ISC_FALSE;
	server->server_id = NULL;

	CHECKFATAL(isc_stats_create(ns_g_mctx, &server->nsstats,
				    dns_nsstatscounter_max),
		   "dns_stats_create (server)");

	CHECKFATAL(dns_rdatatypestats_create(ns_g_mctx,
					     &server->rcvquerystats),
		   "dns_stats_create (rcvquery)");

	CHECKFATAL(dns_opcodestats_create(ns_g_mctx, &server->opcodestats),
		   "dns_stats_create (opcode)");

	CHECKFATAL(isc_stats_create(ns_g_mctx, &server->zonestats,
				    dns_zonestatscounter_max),
		   "dns_stats_create (zone)");

	CHECKFATAL(isc_stats_create(ns_g_mctx, &server->resolverstats,
				    dns_resstatscounter_max),
		   "dns_stats_create (resolver)");

	server->flushonshutdown = ISC_FALSE;
	server->log_queries = ISC_FALSE;

	server->controls = NULL;
	CHECKFATAL(ns_controls_create(server, &server->controls),
		   "ns_controls_create");
	server->dispatchgen = 0;
	ISC_LIST_INIT(server->dispatches);

	ISC_LIST_INIT(server->statschannels);

	server->magic = NS_SERVER_MAGIC;
	*serverp = server;
}

void
ns_server_destroy(ns_server_t **serverp) {
	ns_server_t *server = *serverp;
	REQUIRE(NS_SERVER_VALID(server));

	ns_controls_destroy(&server->controls);

	isc_stats_detach(&server->nsstats);
	dns_stats_detach(&server->rcvquerystats);
	dns_stats_detach(&server->opcodestats);
	isc_stats_detach(&server->zonestats);
	isc_stats_detach(&server->resolverstats);
	isc_stats_detach(&server->sockstats);

	isc_mem_free(server->mctx, server->statsfile);
	isc_mem_free(server->mctx, server->dumpfile);
	isc_mem_free(server->mctx, server->recfile);

	if (server->version != NULL)
		isc_mem_free(server->mctx, server->version);
	if (server->hostname != NULL)
		isc_mem_free(server->mctx, server->hostname);
	if (server->server_id != NULL)
		isc_mem_free(server->mctx, server->server_id);

	dns_zonemgr_detach(&server->zonemgr);

	if (server->tkeyctx != NULL)
		dns_tkeyctx_destroy(&server->tkeyctx);

	dst_lib_destroy();

	isc_event_free(&server->reload_event);

	INSIST(ISC_LIST_EMPTY(server->viewlist));

	dns_aclenv_destroy(&server->aclenv);

	isc_quota_destroy(&server->recursionquota);
	isc_quota_destroy(&server->tcpquota);
	isc_quota_destroy(&server->xfroutquota);

	server->magic = 0;
	isc_mem_put(server->mctx, server, sizeof(*server));
	*serverp = NULL;
}

static void
fatal(const char *msg, isc_result_t result) {
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      ISC_LOG_CRITICAL, "%s: %s", msg,
		      isc_result_totext(result));
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      ISC_LOG_CRITICAL, "exiting (due to fatal error)");
	exit(1);
}

static void
start_reserved_dispatches(ns_server_t *server) {

	REQUIRE(NS_SERVER_VALID(server));

	server->dispatchgen++;
}

static void
end_reserved_dispatches(ns_server_t *server, isc_boolean_t all) {
	ns_dispatch_t *dispatch, *nextdispatch;

	REQUIRE(NS_SERVER_VALID(server));

	for (dispatch = ISC_LIST_HEAD(server->dispatches);
	     dispatch != NULL;
	     dispatch = nextdispatch) {
		nextdispatch = ISC_LIST_NEXT(dispatch, link);
		if (!all && server->dispatchgen == dispatch-> dispatchgen)
			continue;
		ISC_LIST_UNLINK(server->dispatches, dispatch, link);
		dns_dispatch_detach(&dispatch->dispatch);
		isc_mem_put(server->mctx, dispatch, sizeof(*dispatch));
	}
}

void
ns_add_reserved_dispatch(ns_server_t *server, const isc_sockaddr_t *addr) {
	ns_dispatch_t *dispatch;
	in_port_t port;
	char addrbuf[ISC_SOCKADDR_FORMATSIZE];
	isc_result_t result;
	unsigned int attrs, attrmask;

	REQUIRE(NS_SERVER_VALID(server));

	port = isc_sockaddr_getport(addr);
	if (port == 0 || port >= 1024)
		return;

	for (dispatch = ISC_LIST_HEAD(server->dispatches);
	     dispatch != NULL;
	     dispatch = ISC_LIST_NEXT(dispatch, link)) {
		if (isc_sockaddr_equal(&dispatch->addr, addr))
			break;
	}
	if (dispatch != NULL) {
		dispatch->dispatchgen = server->dispatchgen;
		return;
	}

	dispatch = isc_mem_get(server->mctx, sizeof(*dispatch));
	if (dispatch == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}

	dispatch->addr = *addr;
	dispatch->dispatchgen = server->dispatchgen;
	dispatch->dispatch = NULL;

	attrs = 0;
	attrs |= DNS_DISPATCHATTR_UDP;
	switch (isc_sockaddr_pf(addr)) {
	case AF_INET:
		attrs |= DNS_DISPATCHATTR_IPV4;
		break;
	case AF_INET6:
		attrs |= DNS_DISPATCHATTR_IPV6;
		break;
	default:
		result = ISC_R_NOTIMPLEMENTED;
		goto cleanup;
	}
	attrmask = 0;
	attrmask |= DNS_DISPATCHATTR_UDP;
	attrmask |= DNS_DISPATCHATTR_TCP;
	attrmask |= DNS_DISPATCHATTR_IPV4;
	attrmask |= DNS_DISPATCHATTR_IPV6;

	result = dns_dispatch_getudp(ns_g_dispatchmgr, ns_g_socketmgr,
				     ns_g_taskmgr, &dispatch->addr, 4096,
				     1000, 32768, 16411, 16433,
				     attrs, attrmask, &dispatch->dispatch);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	ISC_LIST_INITANDPREPEND(server->dispatches, dispatch, link);

	return;

 cleanup:
	if (dispatch != NULL)
		isc_mem_put(server->mctx, dispatch, sizeof(*dispatch));
	isc_sockaddr_format(addr, addrbuf, sizeof(addrbuf));
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_SERVER, ISC_LOG_WARNING,
		      "unable to create dispatch for reserved port %s: %s",
		      addrbuf, isc_result_totext(result));
}


static isc_result_t
loadconfig(ns_server_t *server) {
	isc_result_t result;
	start_reserved_dispatches(server);
	result = load_configuration(ns_g_lwresdonly ?
				    lwresd_g_conffile : ns_g_conffile,
				    server, ISC_FALSE);
	if (result == ISC_R_SUCCESS) {
		end_reserved_dispatches(server, ISC_FALSE);
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
			      "reloading configuration succeeded");
	} else {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "reloading configuration failed: %s",
			      isc_result_totext(result));
	}
	return (result);
}

static isc_result_t
reload(ns_server_t *server) {
	isc_result_t result;
	CHECK(loadconfig(server));

	result = load_zones(server, ISC_FALSE);
	if (result == ISC_R_SUCCESS)
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
			      "reloading zones succeeded");
	else
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "reloading zones failed: %s",
			      isc_result_totext(result));

 cleanup:
	return (result);
}

static void
reconfig(ns_server_t *server) {
	isc_result_t result;
	CHECK(loadconfig(server));

	result = load_new_zones(server, ISC_FALSE);
	if (result == ISC_R_SUCCESS)
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
			      "any newly configured zones are now loaded");
	else
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "loading new zones failed: %s",
			      isc_result_totext(result));

 cleanup: ;
}

/*
 * Handle a reload event (from SIGHUP).
 */
static void
ns_server_reload(isc_task_t *task, isc_event_t *event) {
	ns_server_t *server = (ns_server_t *)event->ev_arg;

	INSIST(task = server->task);
	UNUSED(task);

	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
		      "received SIGHUP signal to reload zones");
	(void)reload(server);

	LOCK(&server->reload_event_lock);
	INSIST(server->reload_event == NULL);
	server->reload_event = event;
	UNLOCK(&server->reload_event_lock);
}

void
ns_server_reloadwanted(ns_server_t *server) {
	LOCK(&server->reload_event_lock);
	if (server->reload_event != NULL)
		isc_task_send(server->task, &server->reload_event);
	UNLOCK(&server->reload_event_lock);
}

static char *
next_token(char **stringp, const char *delim) {
	char *res;

	do {
		res = strsep(stringp, delim);
		if (res == NULL)
			break;
	} while (*res == '\0');
	return (res);
}

/*
 * Find the zone specified in the control channel command 'args',
 * if any.  If a zone is specified, point '*zonep' at it, otherwise
 * set '*zonep' to NULL.
 */
static isc_result_t
zone_from_args(ns_server_t *server, char *args, dns_zone_t **zonep) {
	char *input, *ptr;
	const char *zonetxt;
	char *classtxt;
	const char *viewtxt = NULL;
	dns_fixedname_t name;
	isc_result_t result;
	isc_buffer_t buf;
	dns_view_t *view = NULL;
	dns_rdataclass_t rdclass;

	REQUIRE(zonep != NULL && *zonep == NULL);

	input = args;

	/* Skip the command name. */
	ptr = next_token(&input, " \t");
	if (ptr == NULL)
		return (ISC_R_UNEXPECTEDEND);

	/* Look for the zone name. */
	zonetxt = next_token(&input, " \t");
	if (zonetxt == NULL)
		return (ISC_R_SUCCESS);

	/* Look for the optional class name. */
	classtxt = next_token(&input, " \t");
	if (classtxt != NULL) {
		/* Look for the optional view name. */
		viewtxt = next_token(&input, " \t");
	}

	isc_buffer_init(&buf, zonetxt, strlen(zonetxt));
	isc_buffer_add(&buf, strlen(zonetxt));
	dns_fixedname_init(&name);
	result = dns_name_fromtext(dns_fixedname_name(&name),
				   &buf, dns_rootname, ISC_FALSE, NULL);
	if (result != ISC_R_SUCCESS)
		goto fail1;

	if (classtxt != NULL) {
		isc_textregion_t r;
		r.base = classtxt;
		r.length = strlen(classtxt);
		result = dns_rdataclass_fromtext(&rdclass, &r);
		if (result != ISC_R_SUCCESS)
			goto fail1;
	} else
		rdclass = dns_rdataclass_in;

	if (viewtxt == NULL) {
		result = dns_viewlist_findzone(&server->viewlist,
					       dns_fixedname_name(&name),
					       ISC_TF(classtxt == NULL),
					       rdclass, zonep);
	} else {
		result = dns_viewlist_find(&server->viewlist, viewtxt,
					   rdclass, &view);
		if (result != ISC_R_SUCCESS)
			goto fail1;

		result = dns_zt_find(view->zonetable, dns_fixedname_name(&name),
				     0, NULL, zonep);
		dns_view_detach(&view);
	}

	/* Partial match? */
	if (result != ISC_R_SUCCESS && *zonep != NULL)
		dns_zone_detach(zonep);
	if (result == DNS_R_PARTIALMATCH)
		result = ISC_R_NOTFOUND;
 fail1:
	return (result);
}

/*
 * Act on a "retransfer" command from the command channel.
 */
isc_result_t
ns_server_retransfercommand(ns_server_t *server, char *args) {
	isc_result_t result;
	dns_zone_t *zone = NULL;
	dns_zonetype_t type;

	result = zone_from_args(server, args, &zone);
	if (result != ISC_R_SUCCESS)
		return (result);
	if (zone == NULL)
		return (ISC_R_UNEXPECTEDEND);
	type = dns_zone_gettype(zone);
	if (type == dns_zone_slave || type == dns_zone_stub)
		dns_zone_forcereload(zone);
	else
		result = ISC_R_NOTFOUND;
	dns_zone_detach(&zone);
	return (result);
}

/*
 * Act on a "reload" command from the command channel.
 */
isc_result_t
ns_server_reloadcommand(ns_server_t *server, char *args, isc_buffer_t *text) {
	isc_result_t result;
	dns_zone_t *zone = NULL;
	dns_zonetype_t type;
	const char *msg = NULL;

	result = zone_from_args(server, args, &zone);
	if (result != ISC_R_SUCCESS)
		return (result);
	if (zone == NULL) {
		result = reload(server);
		if (result == ISC_R_SUCCESS)
			msg = "server reload successful";
	} else {
		type = dns_zone_gettype(zone);
		if (type == dns_zone_slave || type == dns_zone_stub) {
			dns_zone_refresh(zone);
			dns_zone_detach(&zone);
			msg = "zone refresh queued";
		} else {
			result = dns_zone_load(zone);
			dns_zone_detach(&zone);
			switch (result) {
			case ISC_R_SUCCESS:
				 msg = "zone reload successful";
				 break;
			case DNS_R_CONTINUE:
				msg = "zone reload queued";
				result = ISC_R_SUCCESS;
				break;
			case DNS_R_UPTODATE:
				msg = "zone reload up-to-date";
				result = ISC_R_SUCCESS;
				break;
			default:
				/* failure message will be generated by rndc */
				break;
			}
		}
	}
	if (msg != NULL && strlen(msg) < isc_buffer_availablelength(text))
		isc_buffer_putmem(text, (const unsigned char *)msg,
				  strlen(msg) + 1);
	return (result);
}

/*
 * Act on a "reconfig" command from the command channel.
 */
isc_result_t
ns_server_reconfigcommand(ns_server_t *server, char *args) {
	UNUSED(args);

	reconfig(server);
	return (ISC_R_SUCCESS);
}

/*
 * Act on a "notify" command from the command channel.
 */
isc_result_t
ns_server_notifycommand(ns_server_t *server, char *args, isc_buffer_t *text) {
	isc_result_t result;
	dns_zone_t *zone = NULL;
	const unsigned char msg[] = "zone notify queued";

	result = zone_from_args(server, args, &zone);
	if (result != ISC_R_SUCCESS)
		return (result);
	if (zone == NULL)
		return (ISC_R_UNEXPECTEDEND);

	dns_zone_notify(zone);
	dns_zone_detach(&zone);
	if (sizeof(msg) <= isc_buffer_availablelength(text))
		isc_buffer_putmem(text, msg, sizeof(msg));

	return (ISC_R_SUCCESS);
}

/*
 * Act on a "refresh" command from the command channel.
 */
isc_result_t
ns_server_refreshcommand(ns_server_t *server, char *args, isc_buffer_t *text) {
	isc_result_t result;
	dns_zone_t *zone = NULL;
	const unsigned char msg1[] = "zone refresh queued";
	const unsigned char msg2[] = "not a slave or stub zone";
	dns_zonetype_t type;

	result = zone_from_args(server, args, &zone);
	if (result != ISC_R_SUCCESS)
		return (result);
	if (zone == NULL)
		return (ISC_R_UNEXPECTEDEND);

	type = dns_zone_gettype(zone);
	if (type == dns_zone_slave || type == dns_zone_stub) {
		dns_zone_refresh(zone);
		dns_zone_detach(&zone);
		if (sizeof(msg1) <= isc_buffer_availablelength(text))
			isc_buffer_putmem(text, msg1, sizeof(msg1));
		return (ISC_R_SUCCESS);
	}

	dns_zone_detach(&zone);
	if (sizeof(msg2) <= isc_buffer_availablelength(text))
		isc_buffer_putmem(text, msg2, sizeof(msg2));
	return (ISC_R_FAILURE);
}

isc_result_t
ns_server_togglequerylog(ns_server_t *server) {
	server->log_queries = server->log_queries ? ISC_FALSE : ISC_TRUE;

	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
		      "query logging is now %s",
		      server->log_queries ? "on" : "off");
	return (ISC_R_SUCCESS);
}

static isc_result_t
ns_listenlist_fromconfig(const cfg_obj_t *listenlist, const cfg_obj_t *config,
			 cfg_aclconfctx_t *actx,
			 isc_mem_t *mctx, ns_listenlist_t **target)
{
	isc_result_t result;
	const cfg_listelt_t *element;
	ns_listenlist_t *dlist = NULL;

	REQUIRE(target != NULL && *target == NULL);

	result = ns_listenlist_create(mctx, &dlist);
	if (result != ISC_R_SUCCESS)
		return (result);

	for (element = cfg_list_first(listenlist);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		ns_listenelt_t *delt = NULL;
		const cfg_obj_t *listener = cfg_listelt_value(element);
		result = ns_listenelt_fromconfig(listener, config, actx,
						 mctx, &delt);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		ISC_LIST_APPEND(dlist->elts, delt, link);
	}
	*target = dlist;
	return (ISC_R_SUCCESS);

 cleanup:
	ns_listenlist_detach(&dlist);
	return (result);
}

/*
 * Create a listen list from the corresponding configuration
 * data structure.
 */
static isc_result_t
ns_listenelt_fromconfig(const cfg_obj_t *listener, const cfg_obj_t *config,
			cfg_aclconfctx_t *actx,
			isc_mem_t *mctx, ns_listenelt_t **target)
{
	isc_result_t result;
	const cfg_obj_t *portobj;
	in_port_t port;
	ns_listenelt_t *delt = NULL;
	REQUIRE(target != NULL && *target == NULL);

	portobj = cfg_tuple_get(listener, "port");
	if (!cfg_obj_isuint32(portobj)) {
		if (ns_g_port != 0) {
			port = ns_g_port;
		} else {
			result = ns_config_getport(config, &port);
			if (result != ISC_R_SUCCESS)
				return (result);
		}
	} else {
		if (cfg_obj_asuint32(portobj) >= ISC_UINT16_MAX) {
			cfg_obj_log(portobj, ns_g_lctx, ISC_LOG_ERROR,
				    "port value '%u' is out of range",
				    cfg_obj_asuint32(portobj));
			return (ISC_R_RANGE);
		}
		port = (in_port_t)cfg_obj_asuint32(portobj);
	}

	result = ns_listenelt_create(mctx, port, NULL, &delt);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = cfg_acl_fromconfig(cfg_tuple_get(listener, "acl"),
				   config, ns_g_lctx, actx, mctx, 0,
				   &delt->acl);
	if (result != ISC_R_SUCCESS) {
		ns_listenelt_destroy(delt);
		return (result);
	}
	*target = delt;
	return (ISC_R_SUCCESS);
}

isc_result_t
ns_server_dumpstats(ns_server_t *server) {
	isc_result_t result;
	FILE *fp = NULL;

	CHECKMF(isc_stdio_open(server->statsfile, "a", &fp),
		"could not open statistics dump file", server->statsfile);

	result = ns_stats_dump(server, fp);
	CHECK(result);

 cleanup:
	if (fp != NULL)
		(void)isc_stdio_close(fp);
	if (result == ISC_R_SUCCESS)
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
			      "dumpstats complete");
	else
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "dumpstats failed: %s",
			      dns_result_totext(result));
	return (result);
}

static isc_result_t
add_zone_tolist(dns_zone_t *zone, void *uap) {
	struct dumpcontext *dctx = uap;
	struct zonelistentry *zle;

	zle = isc_mem_get(dctx->mctx, sizeof *zle);
	if (zle ==  NULL)
		return (ISC_R_NOMEMORY);
	zle->zone = NULL;
	dns_zone_attach(zone, &zle->zone);
	ISC_LINK_INIT(zle, link);
	ISC_LIST_APPEND(ISC_LIST_TAIL(dctx->viewlist)->zonelist, zle, link);
	return (ISC_R_SUCCESS);
}

static isc_result_t
add_view_tolist(struct dumpcontext *dctx, dns_view_t *view) {
	struct viewlistentry *vle;
	isc_result_t result = ISC_R_SUCCESS;

	/*
	 * Prevent duplicate views.
	 */
	for (vle = ISC_LIST_HEAD(dctx->viewlist);
	     vle != NULL;
	     vle = ISC_LIST_NEXT(vle, link))
		if (vle->view == view)
			return (ISC_R_SUCCESS);

	vle = isc_mem_get(dctx->mctx, sizeof *vle);
	if (vle == NULL)
		return (ISC_R_NOMEMORY);
	vle->view = NULL;
	dns_view_attach(view, &vle->view);
	ISC_LINK_INIT(vle, link);
	ISC_LIST_INIT(vle->zonelist);
	ISC_LIST_APPEND(dctx->viewlist, vle, link);
	if (dctx->dumpzones)
		result = dns_zt_apply(view->zonetable, ISC_TRUE,
				      add_zone_tolist, dctx);
	return (result);
}

static void
dumpcontext_destroy(struct dumpcontext *dctx) {
	struct viewlistentry *vle;
	struct zonelistentry *zle;

	vle = ISC_LIST_HEAD(dctx->viewlist);
	while (vle != NULL) {
		ISC_LIST_UNLINK(dctx->viewlist, vle, link);
		zle = ISC_LIST_HEAD(vle->zonelist);
		while (zle != NULL) {
			ISC_LIST_UNLINK(vle->zonelist, zle, link);
			dns_zone_detach(&zle->zone);
			isc_mem_put(dctx->mctx, zle, sizeof *zle);
			zle = ISC_LIST_HEAD(vle->zonelist);
		}
		dns_view_detach(&vle->view);
		isc_mem_put(dctx->mctx, vle, sizeof *vle);
		vle = ISC_LIST_HEAD(dctx->viewlist);
	}
	if (dctx->version != NULL)
		dns_db_closeversion(dctx->db, &dctx->version, ISC_FALSE);
	if (dctx->db != NULL)
		dns_db_detach(&dctx->db);
	if (dctx->cache != NULL)
		dns_db_detach(&dctx->cache);
	if (dctx->task != NULL)
		isc_task_detach(&dctx->task);
	if (dctx->fp != NULL)
		(void)isc_stdio_close(dctx->fp);
	if (dctx->mdctx != NULL)
		dns_dumpctx_detach(&dctx->mdctx);
	isc_mem_put(dctx->mctx, dctx, sizeof *dctx);
}

static void
dumpdone(void *arg, isc_result_t result) {
	struct dumpcontext *dctx = arg;
	char buf[1024+32];
	const dns_master_style_t *style;

	if (result != ISC_R_SUCCESS)
		goto cleanup;
	if (dctx->mdctx != NULL)
		dns_dumpctx_detach(&dctx->mdctx);
	if (dctx->view == NULL) {
		dctx->view = ISC_LIST_HEAD(dctx->viewlist);
		if (dctx->view == NULL)
			goto done;
		INSIST(dctx->zone == NULL);
	} else
		goto resume;
 nextview:
	fprintf(dctx->fp, ";\n; Start view %s\n;\n", dctx->view->view->name);
 resume:
	if (dctx->zone == NULL && dctx->cache == NULL && dctx->dumpcache) {
		style = &dns_master_style_cache;
		/* start cache dump */
		if (dctx->view->view->cachedb != NULL)
			dns_db_attach(dctx->view->view->cachedb, &dctx->cache);
		if (dctx->cache != NULL) {

			fprintf(dctx->fp, ";\n; Cache dump of view '%s'\n;\n",
				dctx->view->view->name);
			result = dns_master_dumptostreaminc(dctx->mctx,
							    dctx->cache, NULL,
							    style, dctx->fp,
							    dctx->task,
							    dumpdone, dctx,
							    &dctx->mdctx);
			if (result == DNS_R_CONTINUE)
				return;
			if (result == ISC_R_NOTIMPLEMENTED)
				fprintf(dctx->fp, "; %s\n",
					dns_result_totext(result));
			else if (result != ISC_R_SUCCESS)
				goto cleanup;
		}
	}
	if (dctx->cache != NULL) {
		dns_adb_dump(dctx->view->view->adb, dctx->fp);
		dns_resolver_printbadcache(dctx->view->view->resolver,
					   dctx->fp);
		dns_db_detach(&dctx->cache);
	}
	if (dctx->dumpzones) {
		style = &dns_master_style_full;
 nextzone:
		if (dctx->version != NULL)
			dns_db_closeversion(dctx->db, &dctx->version,
					    ISC_FALSE);
		if (dctx->db != NULL)
			dns_db_detach(&dctx->db);
		if (dctx->zone == NULL)
			dctx->zone = ISC_LIST_HEAD(dctx->view->zonelist);
		else
			dctx->zone = ISC_LIST_NEXT(dctx->zone, link);
		if (dctx->zone != NULL) {
			/* start zone dump */
			dns_zone_name(dctx->zone->zone, buf, sizeof(buf));
			fprintf(dctx->fp, ";\n; Zone dump of '%s'\n;\n", buf);
			result = dns_zone_getdb(dctx->zone->zone, &dctx->db);
			if (result != ISC_R_SUCCESS) {
				fprintf(dctx->fp, "; %s\n",
					dns_result_totext(result));
				goto nextzone;
			}
			dns_db_currentversion(dctx->db, &dctx->version);
			result = dns_master_dumptostreaminc(dctx->mctx,
							    dctx->db,
							    dctx->version,
							    style, dctx->fp,
							    dctx->task,
							    dumpdone, dctx,
							    &dctx->mdctx);
			if (result == DNS_R_CONTINUE)
				return;
			if (result == ISC_R_NOTIMPLEMENTED) {
				fprintf(dctx->fp, "; %s\n",
					dns_result_totext(result));
				result = ISC_R_SUCCESS;
				goto nextzone;
			}
			if (result != ISC_R_SUCCESS)
				goto cleanup;
		}
	}
	if (dctx->view != NULL)
		dctx->view = ISC_LIST_NEXT(dctx->view, link);
	if (dctx->view != NULL)
		goto nextview;
 done:
	fprintf(dctx->fp, "; Dump complete\n");
	result = isc_stdio_flush(dctx->fp);
	if (result == ISC_R_SUCCESS)
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
			      "dumpdb complete");
 cleanup:
	if (result != ISC_R_SUCCESS)
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "dumpdb failed: %s", dns_result_totext(result));
	dumpcontext_destroy(dctx);
}

isc_result_t
ns_server_dumpdb(ns_server_t *server, char *args) {
	struct dumpcontext *dctx = NULL;
	dns_view_t *view;
	isc_result_t result;
	char *ptr;
	const char *sep;

	/* Skip the command name. */
	ptr = next_token(&args, " \t");
	if (ptr == NULL)
		return (ISC_R_UNEXPECTEDEND);

	dctx = isc_mem_get(server->mctx, sizeof(*dctx));
	if (dctx == NULL)
		return (ISC_R_NOMEMORY);

	dctx->mctx = server->mctx;
	dctx->dumpcache = ISC_TRUE;
	dctx->dumpzones = ISC_FALSE;
	dctx->fp = NULL;
	ISC_LIST_INIT(dctx->viewlist);
	dctx->view = NULL;
	dctx->zone = NULL;
	dctx->cache = NULL;
	dctx->mdctx = NULL;
	dctx->db = NULL;
	dctx->cache = NULL;
	dctx->task = NULL;
	dctx->version = NULL;
	isc_task_attach(server->task, &dctx->task);

	CHECKMF(isc_stdio_open(server->dumpfile, "w", &dctx->fp),
		"could not open dump file", server->dumpfile);

	sep = (args == NULL) ? "" : ": ";
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
		      "dumpdb started%s%s", sep, (args != NULL) ? args : "");

	ptr = next_token(&args, " \t");
	if (ptr != NULL && strcmp(ptr, "-all") == 0) {
		dctx->dumpzones = ISC_TRUE;
		dctx->dumpcache = ISC_TRUE;
		ptr = next_token(&args, " \t");
	} else if (ptr != NULL && strcmp(ptr, "-cache") == 0) {
		dctx->dumpzones = ISC_FALSE;
		dctx->dumpcache = ISC_TRUE;
		ptr = next_token(&args, " \t");
	} else if (ptr != NULL && strcmp(ptr, "-zones") == 0) {
		dctx->dumpzones = ISC_TRUE;
		dctx->dumpcache = ISC_FALSE;
		ptr = next_token(&args, " \t");
	}

 nextview:
	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link))
	{
		if (ptr != NULL && strcmp(view->name, ptr) != 0)
			continue;
		CHECK(add_view_tolist(dctx, view));
	}
	if (ptr != NULL) {
		ptr = next_token(&args, " \t");
		if (ptr != NULL)
			goto nextview;
	}
	dumpdone(dctx, ISC_R_SUCCESS);
	return (ISC_R_SUCCESS);

 cleanup:
	if (dctx != NULL)
		dumpcontext_destroy(dctx);
	return (result);
}

isc_result_t
ns_server_dumprecursing(ns_server_t *server) {
	FILE *fp = NULL;
	isc_result_t result;

	CHECKMF(isc_stdio_open(server->recfile, "w", &fp),
		"could not open dump file", server->recfile);
	fprintf(fp,";\n; Recursing Queries\n;\n");
	ns_interfacemgr_dumprecursing(fp, server->interfacemgr);
	fprintf(fp, "; Dump complete\n");

 cleanup:
	if (fp != NULL)
		result = isc_stdio_close(fp);
	if (result == ISC_R_SUCCESS)
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
			      "dumprecursing complete");
	else
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "dumprecursing failed: %s",
			      dns_result_totext(result));
	return (result);
}

isc_result_t
ns_server_setdebuglevel(ns_server_t *server, char *args) {
	char *ptr;
	char *levelstr;
	char *endp;
	long newlevel;

	UNUSED(server);

	/* Skip the command name. */
	ptr = next_token(&args, " \t");
	if (ptr == NULL)
		return (ISC_R_UNEXPECTEDEND);

	/* Look for the new level name. */
	levelstr = next_token(&args, " \t");
	if (levelstr == NULL) {
		if (ns_g_debuglevel < 99)
			ns_g_debuglevel++;
	} else {
		newlevel = strtol(levelstr, &endp, 10);
		if (*endp != '\0' || newlevel < 0 || newlevel > 99)
			return (ISC_R_RANGE);
		ns_g_debuglevel = (unsigned int)newlevel;
	}
	isc_log_setdebuglevel(ns_g_lctx, ns_g_debuglevel);
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
		      "debug level is now %d", ns_g_debuglevel);
	return (ISC_R_SUCCESS);
}

isc_result_t
ns_server_validation(ns_server_t *server, char *args) {
	char *ptr, *viewname;
	dns_view_t *view;
	isc_boolean_t changed = ISC_FALSE;
	isc_result_t result;
	isc_boolean_t enable;

	/* Skip the command name. */
	ptr = next_token(&args, " \t");
	if (ptr == NULL)
		return (ISC_R_UNEXPECTEDEND);

	/* Find out what we are to do. */
	ptr = next_token(&args, " \t");
	if (ptr == NULL)
		return (ISC_R_UNEXPECTEDEND);

	if (!strcasecmp(ptr, "on") || !strcasecmp(ptr, "yes") ||
	    !strcasecmp(ptr, "enable") || !strcasecmp(ptr, "true"))
		enable = ISC_TRUE;
	else if (!strcasecmp(ptr, "off") || !strcasecmp(ptr, "no") ||
		 !strcasecmp(ptr, "disable") || !strcasecmp(ptr, "false"))
		enable = ISC_FALSE;
	else
		return (DNS_R_SYNTAX);

	/* Look for the view name. */
	viewname = next_token(&args, " \t");

	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link))
	{
		if (viewname != NULL && strcasecmp(viewname, view->name) != 0)
			continue;
		result = dns_view_flushcache(view);
		if (result != ISC_R_SUCCESS)
			goto out;
		view->enablevalidation = enable;
		changed = ISC_TRUE;
	}
	if (changed)
		result = ISC_R_SUCCESS;
	else
		result = ISC_R_FAILURE;
 out:
	isc_task_endexclusive(server->task);
	return (result);
}

isc_result_t
ns_server_flushcache(ns_server_t *server, char *args) {
	char *ptr, *viewname;
	dns_view_t *view;
	isc_boolean_t flushed;
	isc_boolean_t found;
	isc_result_t result;

	/* Skip the command name. */
	ptr = next_token(&args, " \t");
	if (ptr == NULL)
		return (ISC_R_UNEXPECTEDEND);

	/* Look for the view name. */
	viewname = next_token(&args, " \t");

	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	flushed = ISC_TRUE;
	found = ISC_FALSE;
	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link))
	{
		if (viewname != NULL && strcasecmp(viewname, view->name) != 0)
			continue;
		found = ISC_TRUE;
		result = dns_view_flushcache(view);
		if (result != ISC_R_SUCCESS) {
			flushed = ISC_FALSE;
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
				      "flushing cache in view '%s' failed: %s",
				      view->name, isc_result_totext(result));
		}
	}
	if (flushed && found) {
		if (viewname != NULL)
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
				      "flushing cache in view '%s' succeeded",
				      viewname);
		else
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
				      "flushing caches in all views succeeded");
		result = ISC_R_SUCCESS;
	} else {
		if (!found) {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
				      "flushing cache in view '%s' failed: "
				      "view not found", viewname);
			result = ISC_R_NOTFOUND;
		} else
			result = ISC_R_FAILURE;
	}
	isc_task_endexclusive(server->task);
	return (result);
}

isc_result_t
ns_server_flushname(ns_server_t *server, char *args) {
	char *ptr, *target, *viewname;
	dns_view_t *view;
	isc_boolean_t flushed;
	isc_boolean_t found;
	isc_result_t result;
	isc_buffer_t b;
	dns_fixedname_t fixed;
	dns_name_t *name;

	/* Skip the command name. */
	ptr = next_token(&args, " \t");
	if (ptr == NULL)
		return (ISC_R_UNEXPECTEDEND);

	/* Find the domain name to flush. */
	target = next_token(&args, " \t");
	if (target == NULL)
		return (ISC_R_UNEXPECTEDEND);

	isc_buffer_init(&b, target, strlen(target));
	isc_buffer_add(&b, strlen(target));
	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	result = dns_name_fromtext(name, &b, dns_rootname, ISC_FALSE, NULL);
	if (result != ISC_R_SUCCESS)
		return (result);

	/* Look for the view name. */
	viewname = next_token(&args, " \t");

	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	flushed = ISC_TRUE;
	found = ISC_FALSE;
	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link))
	{
		if (viewname != NULL && strcasecmp(viewname, view->name) != 0)
			continue;
		found = ISC_TRUE;
		result = dns_view_flushname(view, name);
		if (result != ISC_R_SUCCESS) {
			flushed = ISC_FALSE;
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
				      "flushing name '%s' in cache view '%s' "
				      "failed: %s", target, view->name,
				      isc_result_totext(result));
		}
	}
	if (flushed && found) {
		if (viewname != NULL)
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
				      "flushing name '%s' in cache view '%s' "
				      "succeeded", target, viewname);
		else
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
				      "flushing name '%s' in all cache views "
				      "succeeded", target);
		result = ISC_R_SUCCESS;
	} else {
		if (!found)
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
				      "flushing name '%s' in cache view '%s' "
				      "failed: view not found", target,
				      viewname);
		result = ISC_R_FAILURE;
	}
	isc_task_endexclusive(server->task);
	return (result);
}

isc_result_t
ns_server_status(ns_server_t *server, isc_buffer_t *text) {
	int zonecount, xferrunning, xferdeferred, soaqueries;
	unsigned int n;
	const char *ob = "", *cb = "", *alt = "";

	if (ns_g_server->version_set) {
		ob = " (";
		cb = ")";
		if (ns_g_server->version == NULL)
			alt = "version.bind/txt/ch disabled";
		else
			alt = ns_g_server->version;
	}
	zonecount = dns_zonemgr_getcount(server->zonemgr, DNS_ZONESTATE_ANY);
	xferrunning = dns_zonemgr_getcount(server->zonemgr,
					   DNS_ZONESTATE_XFERRUNNING);
	xferdeferred = dns_zonemgr_getcount(server->zonemgr,
					    DNS_ZONESTATE_XFERDEFERRED);
	soaqueries = dns_zonemgr_getcount(server->zonemgr,
					  DNS_ZONESTATE_SOAQUERY);

	n = snprintf((char *)isc_buffer_used(text),
		     isc_buffer_availablelength(text),
		     "version: %s%s%s%s\n"
#ifdef ISC_PLATFORM_USETHREADS
		     "CPUs found: %u\n"
		     "worker threads: %u\n"
#endif
		     "number of zones: %u\n"
		     "debug level: %d\n"
		     "xfers running: %u\n"
		     "xfers deferred: %u\n"
		     "soa queries in progress: %u\n"
		     "query logging is %s\n"
		     "recursive clients: %d/%d/%d\n"
		     "tcp clients: %d/%d\n"
		     "server is up and running",
		     ns_g_version, ob, alt, cb,
#ifdef ISC_PLATFORM_USETHREADS
		     ns_g_cpus_detected, ns_g_cpus,
#endif
		     zonecount, ns_g_debuglevel, xferrunning, xferdeferred,
		     soaqueries, server->log_queries ? "ON" : "OFF",
		     server->recursionquota.used, server->recursionquota.soft,
		     server->recursionquota.max,
		     server->tcpquota.used, server->tcpquota.max);
	if (n >= isc_buffer_availablelength(text))
		return (ISC_R_NOSPACE);
	isc_buffer_add(text, n);
	return (ISC_R_SUCCESS);
}

static isc_result_t
delete_keynames(dns_tsig_keyring_t *ring, char *target,
		unsigned int *foundkeys)
{
	char namestr[DNS_NAME_FORMATSIZE];
	isc_result_t result;
	dns_rbtnodechain_t chain;
	dns_name_t foundname;
	dns_fixedname_t fixedorigin;
	dns_name_t *origin;
	dns_rbtnode_t *node;
	dns_tsigkey_t *tkey;

	dns_name_init(&foundname, NULL);
	dns_fixedname_init(&fixedorigin);
	origin = dns_fixedname_name(&fixedorigin);

 again:
	dns_rbtnodechain_init(&chain, ring->mctx);
	result = dns_rbtnodechain_first(&chain, ring->keys, &foundname,
					origin);
	if (result == ISC_R_NOTFOUND) {
		dns_rbtnodechain_invalidate(&chain);
		return (ISC_R_SUCCESS);
	}
	if (result != ISC_R_SUCCESS && result != DNS_R_NEWORIGIN) {
		dns_rbtnodechain_invalidate(&chain);
		return (result);
	}

	for (;;) {
		node = NULL;
		dns_rbtnodechain_current(&chain, &foundname, origin, &node);
		tkey = node->data;

		if (tkey != NULL) {
			if (!tkey->generated)
				goto nextkey;

			dns_name_format(&tkey->name, namestr, sizeof(namestr));
			if (strcmp(namestr, target) == 0) {
				(*foundkeys)++;
				dns_rbtnodechain_invalidate(&chain);
				(void)dns_rbt_deletename(ring->keys,
							 &tkey->name,
							 ISC_FALSE);
				goto again;
			}
		}

	nextkey:
		result = dns_rbtnodechain_next(&chain, &foundname, origin);
		if (result == ISC_R_NOMORE)
			break;
		if (result != ISC_R_SUCCESS && result != DNS_R_NEWORIGIN) {
			dns_rbtnodechain_invalidate(&chain);
			return (result);
		}
	}

	return (ISC_R_SUCCESS);
}

isc_result_t
ns_server_tsigdelete(ns_server_t *server, char *command, isc_buffer_t *text) {
	isc_result_t result;
	unsigned int n;
	dns_view_t *view;
	unsigned int foundkeys = 0;
	char *target;
	char *viewname;

	(void)next_token(&command, " \t");  /* skip command name */
	target = next_token(&command, " \t");
	if (target == NULL)
		return (ISC_R_UNEXPECTEDEND);
	viewname = next_token(&command, " \t");

	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link)) {
		if (viewname == NULL || strcmp(view->name, viewname) == 0) {
			RWLOCK(&view->dynamickeys->lock, isc_rwlocktype_write);
			result = delete_keynames(view->dynamickeys, target,
						 &foundkeys);
			RWUNLOCK(&view->dynamickeys->lock,
				 isc_rwlocktype_write);
			if (result != ISC_R_SUCCESS) {
				isc_task_endexclusive(server->task);
				return (result);
			}
		}
	}
	isc_task_endexclusive(server->task);

	n = snprintf((char *)isc_buffer_used(text),
		     isc_buffer_availablelength(text),
		     "%d tsig keys deleted.\n", foundkeys);
	if (n >= isc_buffer_availablelength(text))
		return (ISC_R_NOSPACE);
	isc_buffer_add(text, n);

	return (ISC_R_SUCCESS);
}

static isc_result_t
list_keynames(dns_view_t *view, dns_tsig_keyring_t *ring, isc_buffer_t *text,
	     unsigned int *foundkeys)
{
	char namestr[DNS_NAME_FORMATSIZE];
	char creatorstr[DNS_NAME_FORMATSIZE];
	isc_result_t result;
	dns_rbtnodechain_t chain;
	dns_name_t foundname;
	dns_fixedname_t fixedorigin;
	dns_name_t *origin;
	dns_rbtnode_t *node;
	dns_tsigkey_t *tkey;
	unsigned int n;
	const char *viewname;

	if (view != NULL)
		viewname = view->name;
	else
		viewname = "(global)";

	dns_name_init(&foundname, NULL);
	dns_fixedname_init(&fixedorigin);
	origin = dns_fixedname_name(&fixedorigin);
	dns_rbtnodechain_init(&chain, ring->mctx);
	result = dns_rbtnodechain_first(&chain, ring->keys, &foundname,
					origin);
	if (result == ISC_R_NOTFOUND) {
		dns_rbtnodechain_invalidate(&chain);
		return (ISC_R_SUCCESS);
	}
	if (result != ISC_R_SUCCESS && result != DNS_R_NEWORIGIN) {
		dns_rbtnodechain_invalidate(&chain);
		return (result);
	}

	for (;;) {
		node = NULL;
		dns_rbtnodechain_current(&chain, &foundname, origin, &node);
		tkey = node->data;

		if (tkey != NULL) {
			(*foundkeys)++;
			dns_name_format(&tkey->name, namestr, sizeof(namestr));
			if (tkey->generated) {
				dns_name_format(tkey->creator, creatorstr,
						sizeof(creatorstr));
				n = snprintf((char *)isc_buffer_used(text),
					     isc_buffer_availablelength(text),
					     "view \"%s\"; type \"dynamic\"; key \"%s\"; creator \"%s\";\n",
					     viewname, namestr, creatorstr);
			} else {
				n = snprintf((char *)isc_buffer_used(text),
					     isc_buffer_availablelength(text),
					     "view \"%s\"; type \"static\"; key \"%s\";\n",
					     viewname, namestr);
			}
			if (n >= isc_buffer_availablelength(text)) {
				dns_rbtnodechain_invalidate(&chain);
				return (ISC_R_NOSPACE);
			}
			isc_buffer_add(text, n);
		}
		result = dns_rbtnodechain_next(&chain, &foundname, origin);
		if (result == ISC_R_NOMORE)
			break;
		if (result != ISC_R_SUCCESS && result != DNS_R_NEWORIGIN) {
			dns_rbtnodechain_invalidate(&chain);
			return (result);
		}
	}

	return (ISC_R_SUCCESS);
}

isc_result_t
ns_server_tsiglist(ns_server_t *server, isc_buffer_t *text) {
	isc_result_t result;
	unsigned int n;
	dns_view_t *view;
	unsigned int foundkeys = 0;

	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link)) {
		RWLOCK(&view->statickeys->lock, isc_rwlocktype_read);
		result = list_keynames(view, view->statickeys, text,
				       &foundkeys);
		RWUNLOCK(&view->statickeys->lock, isc_rwlocktype_read);
		if (result != ISC_R_SUCCESS) {
			isc_task_endexclusive(server->task);
			return (result);
		}
		RWLOCK(&view->dynamickeys->lock, isc_rwlocktype_read);
		result = list_keynames(view, view->dynamickeys, text,
				       &foundkeys);
		RWUNLOCK(&view->dynamickeys->lock, isc_rwlocktype_read);
		if (result != ISC_R_SUCCESS) {
			isc_task_endexclusive(server->task);
			return (result);
		}
	}
	isc_task_endexclusive(server->task);

	if (foundkeys == 0) {
		n = snprintf((char *)isc_buffer_used(text),
			     isc_buffer_availablelength(text),
			     "no tsig keys found.\n");
		if (n >= isc_buffer_availablelength(text))
			return (ISC_R_NOSPACE);
		isc_buffer_add(text, n);
	}

	return (ISC_R_SUCCESS);
}

/*
 * Act on a "freeze" or "thaw" command from the command channel.
 */
isc_result_t
ns_server_freeze(ns_server_t *server, isc_boolean_t freeze, char *args,
		 isc_buffer_t *text)
{
	isc_result_t result, tresult;
	dns_zone_t *zone = NULL;
	dns_zonetype_t type;
	char classstr[DNS_RDATACLASS_FORMATSIZE];
	char zonename[DNS_NAME_FORMATSIZE];
	dns_view_t *view;
	char *journal;
	const char *vname, *sep;
	isc_boolean_t frozen;
	const char *msg = NULL;

	result = zone_from_args(server, args, &zone);
	if (result != ISC_R_SUCCESS)
		return (result);
	if (zone == NULL) {
		result = isc_task_beginexclusive(server->task);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		tresult = ISC_R_SUCCESS;
		for (view = ISC_LIST_HEAD(server->viewlist);
		     view != NULL;
		     view = ISC_LIST_NEXT(view, link)) {
			result = dns_view_freezezones(view, freeze);
			if (result != ISC_R_SUCCESS &&
			    tresult == ISC_R_SUCCESS)
				tresult = result;
		}
		isc_task_endexclusive(server->task);
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
			      "%s all zones: %s",
			      freeze ? "freezing" : "thawing",
			      isc_result_totext(tresult));
		return (tresult);
	}
	type = dns_zone_gettype(zone);
	if (type != dns_zone_master) {
		dns_zone_detach(&zone);
		return (ISC_R_NOTFOUND);
	}

	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	frozen = dns_zone_getupdatedisabled(zone);
	if (freeze) {
		if (frozen) {
			msg = "WARNING: The zone was already frozen.\n"
			      "Someone else may be editing it or "
			      "it may still be re-loading.";
			result = DNS_R_FROZEN;
		}
		if (result == ISC_R_SUCCESS) {
			result = dns_zone_flush(zone);
			if (result != ISC_R_SUCCESS)
				msg = "Flushing the zone updates to "
				      "disk failed.";
		}
		if (result == ISC_R_SUCCESS) {
			journal = dns_zone_getjournal(zone);
			if (journal != NULL)
				(void)isc_file_remove(journal);
		}
		if (result == ISC_R_SUCCESS)
			dns_zone_setupdatedisabled(zone, freeze);
	} else {
		if (frozen) {
			result = dns_zone_loadandthaw(zone);
			switch (result) {
			case ISC_R_SUCCESS:
			case DNS_R_UPTODATE:
				msg = "The zone reload and thaw was "
				      "successful.";
				result = ISC_R_SUCCESS;
				break;
			case DNS_R_CONTINUE:
				msg = "A zone reload and thaw was started.\n"
				      "Check the logs to see the result.";
				result = ISC_R_SUCCESS;
				break;
			}
		}
	}
	isc_task_endexclusive(server->task);

	if (msg != NULL && strlen(msg) < isc_buffer_availablelength(text))
		isc_buffer_putmem(text, (const unsigned char *)msg,
				  strlen(msg) + 1);

	view = dns_zone_getview(zone);
	if (strcmp(view->name, "_bind") == 0 ||
	    strcmp(view->name, "_default") == 0)
	{
		vname = "";
		sep = "";
	} else {
		vname = view->name;
		sep = " ";
	}
	dns_rdataclass_format(dns_zone_getclass(zone), classstr,
			      sizeof(classstr));
	dns_name_format(dns_zone_getorigin(zone),
			zonename, sizeof(zonename));
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
		      "%s zone '%s/%s'%s%s: %s",
		      freeze ? "freezing" : "thawing",
		      zonename, classstr, sep, vname,
		      isc_result_totext(result));
	dns_zone_detach(&zone);
	return (result);
}

#ifdef HAVE_LIBSCF
/*
 * This function adds a message for rndc to echo if named
 * is managed by smf and is also running chroot.
 */
isc_result_t
ns_smf_add_message(isc_buffer_t *text) {
	unsigned int n;

	n = snprintf((char *)isc_buffer_used(text),
		isc_buffer_availablelength(text),
		"use svcadm(1M) to manage named");
	if (n >= isc_buffer_availablelength(text))
		return (ISC_R_NOSPACE);
	isc_buffer_add(text, n);
	return (ISC_R_SUCCESS);
}
#endif /* HAVE_LIBSCF */
