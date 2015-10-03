/*
 * Copyright (C) 2004-2015  Internet Systems Consortium, Inc. ("ISC")
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

/*! \file */

#include <config.h>

#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <isc/app.h>
#include <isc/base64.h>
#include <isc/dir.h>
#include <isc/entropy.h>
#include <isc/file.h>
#include <isc/hash.h>
#include <isc/hex.h>
#include <isc/httpd.h>
#include <isc/lex.h>
#include <isc/parseint.h>
#include <isc/portset.h>
#include <isc/print.h>
#include <isc/refcount.h>
#include <isc/resource.h>
#include <isc/sha2.h>
#include <isc/socket.h>
#include <isc/stat.h>
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
#include <dns/dlz.h>
#include <dns/dns64.h>
#include <dns/forward.h>
#include <dns/journal.h>
#include <dns/keytable.h>
#include <dns/keyvalues.h>
#include <dns/lib.h>
#include <dns/master.h>
#include <dns/masterdump.h>
#include <dns/order.h>
#include <dns/peer.h>
#include <dns/portlist.h>
#include <dns/private.h>
#include <dns/rbt.h>
#include <dns/rdataclass.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdatastruct.h>
#include <dns/resolver.h>
#include <dns/rootns.h>
#include <dns/secalg.h>
#include <dns/soa.h>
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

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

/*%
 * Check an operation for failure.  Assumes that the function
 * using it has a 'result' variable and a 'cleanup' label.
 */
#define CHECK(op) \
	do { result = (op);					 \
	       if (result != ISC_R_SUCCESS) goto cleanup;	 \
	} while (0)

#define TCHECK(op) \
	do { tresult = (op);					 \
		if (tresult != ISC_R_SUCCESS) {			 \
			isc_buffer_clear(text);			 \
			goto cleanup;	 			 \
		}						 \
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

/*%
 * Maximum ADB size for views that share a cache.  Use this limit to suppress
 * the total of memory footprint, which should be the main reason for sharing
 * a cache.  Only effective when a finite max-cache-size is specified.
 * This is currently defined to be 8MB.
 */
#define MAX_ADB_SIZE_FOR_CACHESHARE	8388608U

struct ns_dispatch {
	isc_sockaddr_t			addr;
	unsigned int			dispatchgen;
	dns_dispatch_t			*dispatch;
	ISC_LINK(struct ns_dispatch)	link;
};

struct ns_cache {
	dns_cache_t			*cache;
	dns_view_t			*primaryview;
	isc_boolean_t			needflush;
	isc_boolean_t			adbsizeadjusted;
	ISC_LINK(ns_cache_t)		link;
};

struct dumpcontext {
	isc_mem_t			*mctx;
	isc_boolean_t			dumpcache;
	isc_boolean_t			dumpzones;
	isc_boolean_t			dumpadb;
	isc_boolean_t			dumpbad;
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

/*%
 * Configuration context to retain for each view that allows
 * new zones to be added at runtime.
 */
struct cfg_context {
	isc_mem_t *			mctx;
	cfg_parser_t *			parser;
	cfg_obj_t *			config;
	cfg_parser_t *			nzparser;
	cfg_obj_t *			nzconfig;
	cfg_aclconfctx_t *		actx;
};

/*%
 * Holds state information for the initial zone loading process.
 * Uses the isc_refcount structure to count the number of views
 * with pending zone loads, dereferencing as each view finishes.
 */
typedef struct {
		ns_server_t *server;
		isc_refcount_t refs;
} ns_zoneload_t;

/*
 * These zones should not leak onto the Internet.
 */
const char *empty_zones[] = {
	/* RFC 1918 */
	"10.IN-ADDR.ARPA",
	"16.172.IN-ADDR.ARPA",
	"17.172.IN-ADDR.ARPA",
	"18.172.IN-ADDR.ARPA",
	"19.172.IN-ADDR.ARPA",
	"20.172.IN-ADDR.ARPA",
	"21.172.IN-ADDR.ARPA",
	"22.172.IN-ADDR.ARPA",
	"23.172.IN-ADDR.ARPA",
	"24.172.IN-ADDR.ARPA",
	"25.172.IN-ADDR.ARPA",
	"26.172.IN-ADDR.ARPA",
	"27.172.IN-ADDR.ARPA",
	"28.172.IN-ADDR.ARPA",
	"29.172.IN-ADDR.ARPA",
	"30.172.IN-ADDR.ARPA",
	"31.172.IN-ADDR.ARPA",
	"168.192.IN-ADDR.ARPA",

	/* RFC 6598 */
	"64.100.IN-ADDR.ARPA",
	"65.100.IN-ADDR.ARPA",
	"66.100.IN-ADDR.ARPA",
	"67.100.IN-ADDR.ARPA",
	"68.100.IN-ADDR.ARPA",
	"69.100.IN-ADDR.ARPA",
	"70.100.IN-ADDR.ARPA",
	"71.100.IN-ADDR.ARPA",
	"72.100.IN-ADDR.ARPA",
	"73.100.IN-ADDR.ARPA",
	"74.100.IN-ADDR.ARPA",
	"75.100.IN-ADDR.ARPA",
	"76.100.IN-ADDR.ARPA",
	"77.100.IN-ADDR.ARPA",
	"78.100.IN-ADDR.ARPA",
	"79.100.IN-ADDR.ARPA",
	"80.100.IN-ADDR.ARPA",
	"81.100.IN-ADDR.ARPA",
	"82.100.IN-ADDR.ARPA",
	"83.100.IN-ADDR.ARPA",
	"84.100.IN-ADDR.ARPA",
	"85.100.IN-ADDR.ARPA",
	"86.100.IN-ADDR.ARPA",
	"87.100.IN-ADDR.ARPA",
	"88.100.IN-ADDR.ARPA",
	"89.100.IN-ADDR.ARPA",
	"90.100.IN-ADDR.ARPA",
	"91.100.IN-ADDR.ARPA",
	"92.100.IN-ADDR.ARPA",
	"93.100.IN-ADDR.ARPA",
	"94.100.IN-ADDR.ARPA",
	"95.100.IN-ADDR.ARPA",
	"96.100.IN-ADDR.ARPA",
	"97.100.IN-ADDR.ARPA",
	"98.100.IN-ADDR.ARPA",
	"99.100.IN-ADDR.ARPA",
	"100.100.IN-ADDR.ARPA",
	"101.100.IN-ADDR.ARPA",
	"102.100.IN-ADDR.ARPA",
	"103.100.IN-ADDR.ARPA",
	"104.100.IN-ADDR.ARPA",
	"105.100.IN-ADDR.ARPA",
	"106.100.IN-ADDR.ARPA",
	"107.100.IN-ADDR.ARPA",
	"108.100.IN-ADDR.ARPA",
	"109.100.IN-ADDR.ARPA",
	"110.100.IN-ADDR.ARPA",
	"111.100.IN-ADDR.ARPA",
	"112.100.IN-ADDR.ARPA",
	"113.100.IN-ADDR.ARPA",
	"114.100.IN-ADDR.ARPA",
	"115.100.IN-ADDR.ARPA",
	"116.100.IN-ADDR.ARPA",
	"117.100.IN-ADDR.ARPA",
	"118.100.IN-ADDR.ARPA",
	"119.100.IN-ADDR.ARPA",
	"120.100.IN-ADDR.ARPA",
	"121.100.IN-ADDR.ARPA",
	"122.100.IN-ADDR.ARPA",
	"123.100.IN-ADDR.ARPA",
	"124.100.IN-ADDR.ARPA",
	"125.100.IN-ADDR.ARPA",
	"126.100.IN-ADDR.ARPA",
	"127.100.IN-ADDR.ARPA",

	/* RFC 5735 and RFC 5737 */
	"0.IN-ADDR.ARPA",	/* THIS NETWORK */
	"127.IN-ADDR.ARPA",	/* LOOPBACK */
	"254.169.IN-ADDR.ARPA",	/* LINK LOCAL */
	"2.0.192.IN-ADDR.ARPA",	/* TEST NET */
	"100.51.198.IN-ADDR.ARPA",	/* TEST NET 2 */
	"113.0.203.IN-ADDR.ARPA",	/* TEST NET 3 */
	"255.255.255.255.IN-ADDR.ARPA",	/* BROADCAST */

	/* Local IPv6 Unicast Addresses */
	"0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.IP6.ARPA",
	"1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.IP6.ARPA",
	/* LOCALLY ASSIGNED LOCAL ADDRESS SCOPE */
	"D.F.IP6.ARPA",
	"8.E.F.IP6.ARPA",	/* LINK LOCAL */
	"9.E.F.IP6.ARPA",	/* LINK LOCAL */
	"A.E.F.IP6.ARPA",	/* LINK LOCAL */
	"B.E.F.IP6.ARPA",	/* LINK LOCAL */

	/* Example Prefix, RFC 3849. */
	"8.B.D.0.1.0.0.2.IP6.ARPA",

	/* RFC 7534 */
	"EMPTY.AS112.ARPA",

	NULL
};

ISC_PLATFORM_NORETURN_PRE static void
fatal(const char *msg, isc_result_t result) ISC_PLATFORM_NORETURN_POST;

static void
ns_server_reload(isc_task_t *task, isc_event_t *event);

static isc_result_t
ns_listenelt_fromconfig(const cfg_obj_t *listener, const cfg_obj_t *config,
			cfg_aclconfctx_t *actx, isc_mem_t *mctx,
			isc_uint16_t family, ns_listenelt_t **target);
static isc_result_t
ns_listenlist_fromconfig(const cfg_obj_t *listenlist, const cfg_obj_t *config,
			 cfg_aclconfctx_t *actx, isc_mem_t *mctx,
			 isc_uint16_t family, ns_listenlist_t **target);

static isc_result_t
configure_forward(const cfg_obj_t *config, dns_view_t *view, dns_name_t *origin,
		  const cfg_obj_t *forwarders, const cfg_obj_t *forwardtype);

static isc_result_t
configure_alternates(const cfg_obj_t *config, dns_view_t *view,
		     const cfg_obj_t *alternates);

static isc_result_t
configure_zone(const cfg_obj_t *config, const cfg_obj_t *zconfig,
	       const cfg_obj_t *vconfig, isc_mem_t *mctx, dns_view_t *view,
	       cfg_aclconfctx_t *aclconf, isc_boolean_t added);

static isc_result_t
add_keydata_zone(dns_view_t *view, const char *directory, isc_mem_t *mctx);

static void
end_reserved_dispatches(ns_server_t *server, isc_boolean_t all);

static void
newzone_cfgctx_destroy(void **cfgp);

static isc_result_t
putstr(isc_buffer_t *b, const char *str);

static isc_result_t
putnull(isc_buffer_t *b);

isc_result_t
add_comment(FILE *fp, const char *viewname);

/*%
 * Configure a single view ACL at '*aclp'.  Get its configuration from
 * 'vconfig' (for per-view configuration) and maybe from 'config'
 */
static isc_result_t
configure_view_acl(const cfg_obj_t *vconfig, const cfg_obj_t *config,
		   const char *aclname, const char *acltuplename,
		   cfg_aclconfctx_t *actx, isc_mem_t *mctx, dns_acl_t **aclp)
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

	if (acltuplename != NULL) {
		/*
		 * If the ACL is given in an optional tuple, retrieve it.
		 * The parser should have ensured that a valid object be
		 * returned.
		 */
		aclobj = cfg_tuple_get(aclobj, acltuplename);
	}

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
configure_view_nametable(const cfg_obj_t *vconfig, const cfg_obj_t *config,
			 const char *confname, const char *conftuplename,
			 isc_mem_t *mctx, dns_rbt_t **rbtp)
{
	isc_result_t result;
	const cfg_obj_t *maps[3];
	const cfg_obj_t *obj = NULL;
	const cfg_listelt_t *element;
	int i = 0;
	dns_fixedname_t fixed;
	dns_name_t *name;
	isc_buffer_t b;
	const char *str;
	const cfg_obj_t *nameobj;

	if (*rbtp != NULL)
		dns_rbt_destroy(rbtp);
	if (vconfig != NULL)
		maps[i++] = cfg_tuple_get(vconfig, "options");
	if (config != NULL) {
		const cfg_obj_t *options = NULL;
		(void)cfg_map_get(config, "options", &options);
		if (options != NULL)
			maps[i++] = options;
	}
	maps[i] = NULL;

	(void)ns_config_get(maps, confname, &obj);
	if (obj == NULL)
		/*
		 * No value available.	*rbtp == NULL.
		 */
		return (ISC_R_SUCCESS);

	if (conftuplename != NULL) {
		obj = cfg_tuple_get(obj, conftuplename);
		if (cfg_obj_isvoid(obj))
			return (ISC_R_SUCCESS);
	}

	result = dns_rbt_create(mctx, NULL, NULL, rbtp);
	if (result != ISC_R_SUCCESS)
		return (result);

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	for (element = cfg_list_first(obj);
	     element != NULL;
	     element = cfg_list_next(element)) {
		nameobj = cfg_listelt_value(element);
		str = cfg_obj_asstring(nameobj);
		isc_buffer_constinit(&b, str, strlen(str));
		isc_buffer_add(&b, strlen(str));
		CHECK(dns_name_fromtext(name, &b, dns_rootname, 0, NULL));
		/*
		 * We don't need the node data, but need to set dummy data to
		 * avoid a partial match with an empty node.  For example, if
		 * we have foo.example.com and bar.example.com, we'd get a match
		 * for baz.example.com, which is not the expected result.
		 * We simply use (void *)1 as the dummy data.
		 */
		result = dns_rbt_addname(*rbtp, name, (void *)1);
		if (result != ISC_R_SUCCESS) {
			cfg_obj_log(nameobj, ns_g_lctx, ISC_LOG_ERROR,
				    "failed to add %s for %s: %s",
				    str, confname, isc_result_totext(result));
			goto cleanup;
		}

	}

	return (result);

  cleanup:
	dns_rbt_destroy(rbtp);
	return (result);

}

static isc_result_t
dstkey_fromconfig(const cfg_obj_t *vconfig, const cfg_obj_t *key,
		  isc_boolean_t managed, dst_key_t **target, isc_mem_t *mctx)
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

	INSIST(target != NULL && *target == NULL);

	flags = cfg_obj_asuint32(cfg_tuple_get(key, "flags"));
	proto = cfg_obj_asuint32(cfg_tuple_get(key, "protocol"));
	alg = cfg_obj_asuint32(cfg_tuple_get(key, "algorithm"));
	keyname = dns_fixedname_name(&fkeyname);
	keynamestr = cfg_obj_asstring(cfg_tuple_get(key, "name"));

	if (managed) {
		const char *initmethod;
		initmethod = cfg_obj_asstring(cfg_tuple_get(key, "init"));

		if (strcasecmp(initmethod, "initial-key") != 0) {
			cfg_obj_log(key, ns_g_lctx, ISC_LOG_ERROR,
				    "managed key '%s': "
				    "invalid initialization method '%s'",
				    keynamestr, initmethod);
			result = ISC_R_FAILURE;
			goto cleanup;
		}
	}

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
			    "%s key '%s' has a weak exponent",
			    managed ? "managed" : "trusted",
			    keynamestr);

	CHECK(dns_rdata_fromstruct(NULL,
				   keystruct.common.rdclass,
				   keystruct.common.rdtype,
				   &keystruct, &rrdatabuf));
	dns_fixedname_init(&fkeyname);
	isc_buffer_constinit(&namebuf, keynamestr, strlen(keynamestr));
	isc_buffer_add(&namebuf, strlen(keynamestr));
	CHECK(dns_name_fromtext(keyname, &namebuf, dns_rootname, 0, NULL));
	CHECK(dst_key_fromdns(keyname, viewclass, &rrdatabuf,
			      mctx, &dstkey));

	*target = dstkey;
	return (ISC_R_SUCCESS);

 cleanup:
	if (result == DST_R_NOCRYPTO) {
		cfg_obj_log(key, ns_g_lctx, ISC_LOG_ERROR,
			    "ignoring %s key for '%s': no crypto support",
			    managed ? "managed" : "trusted",
			    keynamestr);
	} else if (result == DST_R_UNSUPPORTEDALG) {
		cfg_obj_log(key, ns_g_lctx, ISC_LOG_WARNING,
			    "skipping %s key for '%s': %s",
			    managed ? "managed" : "trusted",
			    keynamestr, isc_result_totext(result));
	} else {
		cfg_obj_log(key, ns_g_lctx, ISC_LOG_ERROR,
			    "configuring %s key for '%s': %s",
			    managed ? "managed" : "trusted",
			    keynamestr, isc_result_totext(result));
		result = ISC_R_FAILURE;
	}

	if (dstkey != NULL)
		dst_key_free(&dstkey);

	return (result);
}

static isc_result_t
load_view_keys(const cfg_obj_t *keys, const cfg_obj_t *vconfig,
	       dns_view_t *view, isc_boolean_t managed,
	       dns_name_t *keyname, isc_mem_t *mctx)
{
	const cfg_listelt_t *elt, *elt2;
	const cfg_obj_t *key, *keylist;
	dst_key_t *dstkey = NULL;
	isc_result_t result;
	dns_keytable_t *secroots = NULL;

	CHECK(dns_view_getsecroots(view, &secroots));

	for (elt = cfg_list_first(keys);
	     elt != NULL;
	     elt = cfg_list_next(elt)) {
		keylist = cfg_listelt_value(elt);

		for (elt2 = cfg_list_first(keylist);
		     elt2 != NULL;
		     elt2 = cfg_list_next(elt2)) {
			key = cfg_listelt_value(elt2);
			result = dstkey_fromconfig(vconfig, key, managed,
						   &dstkey, mctx);
			if (result ==  DST_R_UNSUPPORTEDALG) {
				result = ISC_R_SUCCESS;
				continue;
			}
			if (result != ISC_R_SUCCESS)
				goto cleanup;

			/*
			 * If keyname was specified, we only add that key.
			 */
			if (keyname != NULL &&
			    !dns_name_equal(keyname, dst_key_name(dstkey)))
			{
				dst_key_free(&dstkey);
				continue;
			}

			CHECK(dns_keytable_add(secroots, managed, &dstkey));
		}
	}

 cleanup:
	if (dstkey != NULL)
		dst_key_free(&dstkey);
	if (secroots != NULL)
		dns_keytable_detach(&secroots);
	if (result == DST_R_NOCRYPTO)
		result = ISC_R_SUCCESS;
	return (result);
}

/*%
 * Configure DNSSEC keys for a view.
 *
 * The per-view configuration values and the server-global defaults are read
 * from 'vconfig' and 'config'.
 */
static isc_result_t
configure_view_dnsseckeys(dns_view_t *view, const cfg_obj_t *vconfig,
			  const cfg_obj_t *config, const cfg_obj_t *bindkeys,
			  isc_boolean_t auto_dlv, isc_boolean_t auto_root,
			  isc_mem_t *mctx)
{
	isc_result_t result = ISC_R_SUCCESS;
	const cfg_obj_t *view_keys = NULL;
	const cfg_obj_t *global_keys = NULL;
	const cfg_obj_t *view_managed_keys = NULL;
	const cfg_obj_t *global_managed_keys = NULL;
	const cfg_obj_t *maps[4];
	const cfg_obj_t *voptions = NULL;
	const cfg_obj_t *options = NULL;
	const cfg_obj_t *obj = NULL;
	const char *directory;
	int i = 0;

	/* We don't need trust anchors for the _bind view */
	if (strcmp(view->name, "_bind") == 0 &&
	    view->rdclass == dns_rdataclass_chaos) {
		return (ISC_R_SUCCESS);
	}

	if (vconfig != NULL) {
		voptions = cfg_tuple_get(vconfig, "options");
		if (voptions != NULL) {
			(void) cfg_map_get(voptions, "trusted-keys",
					   &view_keys);
			(void) cfg_map_get(voptions, "managed-keys",
					   &view_managed_keys);
			maps[i++] = voptions;
		}
	}

	if (config != NULL) {
		(void)cfg_map_get(config, "trusted-keys", &global_keys);
		(void)cfg_map_get(config, "managed-keys", &global_managed_keys);
		(void)cfg_map_get(config, "options", &options);
		if (options != NULL) {
			maps[i++] = options;
		}
	}

	maps[i++] = ns_g_defaults;
	maps[i] = NULL;

	result = dns_view_initsecroots(view, mctx);
	if (result != ISC_R_SUCCESS) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "couldn't create keytable");
		return (ISC_R_UNEXPECTED);
	}

	if (auto_dlv && view->rdclass == dns_rdataclass_in) {
		const cfg_obj_t *builtin_keys = NULL;
		const cfg_obj_t *builtin_managed_keys = NULL;

		isc_log_write(ns_g_lctx, DNS_LOGCATEGORY_SECURITY,
			      NS_LOGMODULE_SERVER, ISC_LOG_WARNING,
			      "using built-in DLV key for view %s",
			      view->name);

		/*
		 * If bind.keys exists, it overrides the managed-keys
		 * clause hard-coded in ns_g_config.
		 */
		if (bindkeys != NULL) {
			(void)cfg_map_get(bindkeys, "trusted-keys",
					  &builtin_keys);
			(void)cfg_map_get(bindkeys, "managed-keys",
					  &builtin_managed_keys);
		} else {
			(void)cfg_map_get(ns_g_config, "trusted-keys",
					  &builtin_keys);
			(void)cfg_map_get(ns_g_config, "managed-keys",
					  &builtin_managed_keys);
		}

		if (builtin_keys != NULL)
			CHECK(load_view_keys(builtin_keys, vconfig, view,
					     ISC_FALSE, view->dlv, mctx));
		if (builtin_managed_keys != NULL)
			CHECK(load_view_keys(builtin_managed_keys, vconfig,
					     view, ISC_TRUE, view->dlv, mctx));
	}

	if (auto_root && view->rdclass == dns_rdataclass_in) {
		const cfg_obj_t *builtin_keys = NULL;
		const cfg_obj_t *builtin_managed_keys = NULL;

		isc_log_write(ns_g_lctx, DNS_LOGCATEGORY_SECURITY,
			      NS_LOGMODULE_SERVER, ISC_LOG_WARNING,
			      "using built-in root key for view %s",
			      view->name);

		/*
		 * If bind.keys exists, it overrides the managed-keys
		 * clause hard-coded in ns_g_config.
		 */
		if (bindkeys != NULL) {
			(void)cfg_map_get(bindkeys, "trusted-keys",
					  &builtin_keys);
			(void)cfg_map_get(bindkeys, "managed-keys",
					  &builtin_managed_keys);
		} else {
			(void)cfg_map_get(ns_g_config, "trusted-keys",
					  &builtin_keys);
			(void)cfg_map_get(ns_g_config, "managed-keys",
					  &builtin_managed_keys);
		}

		if (builtin_keys != NULL)
			CHECK(load_view_keys(builtin_keys, vconfig, view,
					     ISC_FALSE, dns_rootname, mctx));
		if (builtin_managed_keys != NULL)
			CHECK(load_view_keys(builtin_managed_keys, vconfig,
					     view, ISC_TRUE, dns_rootname,
					     mctx));
	}

	CHECK(load_view_keys(view_keys, vconfig, view, ISC_FALSE,
			     NULL, mctx));
	CHECK(load_view_keys(view_managed_keys, vconfig, view, ISC_TRUE,
			     NULL, mctx));

	if (view->rdclass == dns_rdataclass_in) {
		CHECK(load_view_keys(global_keys, vconfig, view, ISC_FALSE,
				     NULL, mctx));
		CHECK(load_view_keys(global_managed_keys, vconfig, view,
				     ISC_TRUE, NULL, mctx));
	}

	/*
	 * Add key zone for managed-keys.
	 */
	obj = NULL;
	(void)ns_config_get(maps, "managed-keys-directory", &obj);
	directory = (obj != NULL ? cfg_obj_asstring(obj) : NULL);
	if (directory != NULL)
		result = isc_file_isdirectory(directory);
	if (result != ISC_R_SUCCESS) {
		isc_log_write(ns_g_lctx, DNS_LOGCATEGORY_SECURITY,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "invalid managed-keys-directory %s: %s",
			      directory, isc_result_totext(result));
		goto cleanup;

	}
	CHECK(add_keydata_zone(view, directory, ns_g_mctx));

  cleanup:
	return (result);
}

static isc_result_t
mustbesecure(const cfg_obj_t *mbs, dns_resolver_t *resolver) {
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
		isc_buffer_constinit(&b, str, strlen(str));
		isc_buffer_add(&b, strlen(str));
		CHECK(dns_name_fromtext(name, &b, dns_rootname, 0, NULL));
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
	isc_result_t result = ISC_R_FAILURE;
	dns_dispatch_t *disp;
	isc_sockaddr_t sa;
	unsigned int attrs, attrmask;
	const cfg_obj_t *obj = NULL;
	unsigned int maxdispatchbuffers;

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
	isc_buffer_constinit(&b, str, strlen(str));
	isc_buffer_add(&b, strlen(str));
	dns_fixedname_init(&fixed);
	result = dns_name_fromtext(dns_fixedname_name(&fixed), &b,
				   dns_rootname, 0, NULL);
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
	isc_buffer_constinit(&b, str, strlen(str));
	isc_buffer_add(&b, strlen(str));
	CHECK(dns_name_fromtext(name, &b, dns_rootname, 0, NULL));

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
		isc_buffer_constinit(&b, str, strlen(str));
		isc_buffer_add(&b, strlen(str));
		result = dns_name_fromtext(name, &b, dns_rootname,
					   0, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		if (dns_name_equal(name, zonename))
			return (ISC_TRUE);
	}
	return (ISC_FALSE);
}

static isc_result_t
check_dbtype(dns_zone_t *zone, unsigned int dbtypec, const char **dbargv,
	     isc_mem_t *mctx)
{
	char **argv = NULL;
	unsigned int i;
	isc_result_t result = ISC_R_SUCCESS;

	CHECK(dns_zone_getdbtype(zone, &argv, mctx));

	/*
	 * Check that all the arguments match.
	 */
	for (i = 0; i < dbtypec; i++)
		if (argv[i] == NULL || strcmp(argv[i], dbargv[i]) != 0)
			CHECK(ISC_R_FAILURE);

	/*
	 * Check that there are not extra arguments.
	 */
	if (i == dbtypec && argv[i] != NULL)
		result = ISC_R_FAILURE;

 cleanup:
	isc_mem_free(mctx, argv);
	return (result);
}

static isc_result_t
setquerystats(dns_zone_t *zone, isc_mem_t *mctx, dns_zonestat_level_t level) {
	isc_result_t result;
	isc_stats_t *zoneqrystats;

	dns_zone_setstatlevel(zone, level);

	zoneqrystats = NULL;
	if (level == dns_zonestat_full) {
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

static ns_cache_t *
cachelist_find(ns_cachelist_t *cachelist, const char *cachename) {
	ns_cache_t *nsc;

	for (nsc = ISC_LIST_HEAD(*cachelist);
	     nsc != NULL;
	     nsc = ISC_LIST_NEXT(nsc, link)) {
		if (strcmp(dns_cache_getname(nsc->cache), cachename) == 0)
			return (nsc);
	}

	return (NULL);
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

static isc_boolean_t
cache_sharable(dns_view_t *originview, dns_view_t *view,
	       isc_boolean_t new_zero_no_soattl,
	       unsigned int new_cleaning_interval,
	       isc_uint64_t new_max_cache_size)
{
	/*
	 * If the cache cannot even reused for the same view, it cannot be
	 * shared with other views.
	 */
	if (!cache_reusable(originview, view, new_zero_no_soattl))
		return (ISC_FALSE);

	/*
	 * Check other cache related parameters that must be consistent among
	 * the sharing views.
	 */
	if (dns_cache_getcleaninginterval(originview->cache) !=
	    new_cleaning_interval ||
	    dns_cache_getcachesize(originview->cache) != new_max_cache_size) {
		return (ISC_FALSE);
	}

	return (ISC_TRUE);
}

/*
 * Callback from DLZ configure when the driver sets up a writeable zone
 */
static isc_result_t
dlzconfigure_callback(dns_view_t *view, dns_zone_t *zone) {
	dns_name_t *origin = dns_zone_getorigin(zone);
	dns_rdataclass_t zclass = view->rdclass;
	isc_result_t result;

	result = dns_zonemgr_managezone(ns_g_server->zonemgr, zone);
	if (result != ISC_R_SUCCESS)
		return (result);
	dns_zone_setstats(zone, ns_g_server->zonestats);

	return (ns_zone_configure_writeable_dlz(view->dlzdatabase,
						zone, zclass, origin));
}

static isc_result_t
dns64_reverse(dns_view_t *view, isc_mem_t *mctx, isc_netaddr_t *na,
	      unsigned int prefixlen, const char *server,
	      const char *contact)
{
	char *cp;
	char reverse[48+sizeof("ip6.arpa.")];
	const char *dns64_dbtype[4] = { "_dns64", "dns64", ".", "." };
	const char *sep = ": view ";
	const char *viewname = view->name;
	const unsigned char *s6;
	dns_fixedname_t fixed;
	dns_name_t *name;
	dns_zone_t *zone = NULL;
	int dns64_dbtypec = 4;
	isc_buffer_t b;
	isc_result_t result;

	REQUIRE(prefixlen == 32 || prefixlen == 40 || prefixlen == 48 ||
		prefixlen == 56 || prefixlen == 64 || prefixlen == 96);

	if (!strcmp(viewname, "_default")) {
		sep = "";
		viewname = "";
	}

	/*
	 * Construct the reverse name of the zone.
	 */
	cp = reverse;
	s6 = na->type.in6.s6_addr;
	while (prefixlen > 0) {
		prefixlen -= 8;
		sprintf(cp, "%x.%x.", s6[prefixlen/8] & 0xf,
			(s6[prefixlen/8] >> 4) & 0xf);
		cp += 4;
	}
	strcat(cp, "ip6.arpa.");

	/*
	 * Create the actual zone.
	 */
	if (server != NULL)
		dns64_dbtype[2] = server;
	if (contact != NULL)
		dns64_dbtype[3] = contact;
	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	isc_buffer_constinit(&b, reverse, strlen(reverse));
	isc_buffer_add(&b, strlen(reverse));
	CHECK(dns_name_fromtext(name, &b, dns_rootname, 0, NULL));
	CHECK(dns_zone_create(&zone, mctx));
	CHECK(dns_zone_setorigin(zone, name));
	dns_zone_setview(zone, view);
	CHECK(dns_zonemgr_managezone(ns_g_server->zonemgr, zone));
	dns_zone_setclass(zone, view->rdclass);
	dns_zone_settype(zone, dns_zone_master);
	dns_zone_setstats(zone, ns_g_server->zonestats);
	CHECK(dns_zone_setdbtype(zone, dns64_dbtypec, dns64_dbtype));
	if (view->queryacl != NULL)
		dns_zone_setqueryacl(zone, view->queryacl);
	if (view->queryonacl != NULL)
		dns_zone_setqueryonacl(zone, view->queryonacl);
	dns_zone_setdialup(zone, dns_dialuptype_no);
	dns_zone_setnotifytype(zone, dns_notifytype_no);
	dns_zone_setoption(zone, DNS_ZONEOPT_NOCHECKNS, ISC_TRUE);
	CHECK(setquerystats(zone, mctx, dns_zonestat_none));	/* XXXMPA */
	CHECK(dns_view_addzone(view, zone));
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      ISC_LOG_INFO, "dns64 reverse zone%s%s: %s", sep,
		      viewname, reverse);

cleanup:
	if (zone != NULL)
		dns_zone_detach(&zone);
	return (result);
}

static isc_result_t
configure_rpz_name(dns_view_t *view, const cfg_obj_t *obj, dns_name_t *name,
		   const char *str, const char *msg)
{
	isc_result_t result;

	result = dns_name_fromstring(name, str, DNS_NAME_DOWNCASE, view->mctx);
	if (result != ISC_R_SUCCESS)
		cfg_obj_log(obj, ns_g_lctx, DNS_RPZ_ERROR_LEVEL,
			    "invalid %s '%s'", msg, str);
	return (result);
}

static isc_result_t
configure_rpz_name2(dns_view_t *view, const cfg_obj_t *obj, dns_name_t *name,
		    const char *str, const dns_name_t *origin)
{
	isc_result_t result;

	result = dns_name_fromstring2(name, str, origin, DNS_NAME_DOWNCASE,
				      view->mctx);
	if (result != ISC_R_SUCCESS)
		cfg_obj_log(obj, ns_g_lctx, DNS_RPZ_ERROR_LEVEL,
			    "invalid zone '%s'", str);
	return (result);
}

static isc_result_t
configure_rpz(dns_view_t *view, const cfg_listelt_t *element,
	      isc_boolean_t recursive_only_def, dns_ttl_t ttl_def)
{
	const cfg_obj_t *rpz_obj, *obj;
	const char *str;
	dns_rpz_zone_t *old, *new;
	isc_result_t result;

	rpz_obj = cfg_listelt_value(element);

	new = isc_mem_get(view->mctx, sizeof(*new));
	if (new == NULL) {
		cfg_obj_log(rpz_obj, ns_g_lctx, DNS_RPZ_ERROR_LEVEL,
			    "no memory for response policy zones");
		return (ISC_R_NOMEMORY);
	}

	memset(new, 0, sizeof(*new));
	dns_name_init(&new->origin, NULL);
	dns_name_init(&new->nsdname, NULL);
	dns_name_init(&new->passthru, NULL);
	dns_name_init(&new->cname, NULL);
	ISC_LIST_INITANDAPPEND(view->rpz_zones, new, link);

	obj = cfg_tuple_get(rpz_obj, "recursive-only");
	if (cfg_obj_isvoid(obj)) {
		new->recursive_only = recursive_only_def;
	} else {
		new->recursive_only = cfg_obj_asboolean(obj);
	}
	if (!new->recursive_only)
		view->rpz_recursive_only = ISC_FALSE;

	obj = cfg_tuple_get(rpz_obj, "max-policy-ttl");
	if (cfg_obj_isuint32(obj)) {
		new->max_policy_ttl = cfg_obj_asuint32(obj);
	} else {
		new->max_policy_ttl = ttl_def;
	}

	str = cfg_obj_asstring(cfg_tuple_get(rpz_obj, "zone name"));
	result = configure_rpz_name(view, rpz_obj, &new->origin, str, "zone");
	if (result != ISC_R_SUCCESS)
		return (result);
	if (dns_name_equal(&new->origin, dns_rootname)) {
		cfg_obj_log(rpz_obj, ns_g_lctx, DNS_RPZ_ERROR_LEVEL,
			    "invalid zone name '%s'", str);
		return (DNS_R_EMPTYLABEL);
	}
	for (old = ISC_LIST_HEAD(view->rpz_zones);
	     old != new;
	     old = ISC_LIST_NEXT(old, link)) {
		++new->num;
		if (dns_name_equal(&old->origin, &new->origin)) {
			cfg_obj_log(rpz_obj, ns_g_lctx, DNS_RPZ_ERROR_LEVEL,
				    "duplicate '%s'", str);
			result = DNS_R_DUPLICATE;
			return (result);
		}
	}

	result = configure_rpz_name2(view, rpz_obj, &new->nsdname,
				     DNS_RPZ_NSDNAME_ZONE, &new->origin);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = configure_rpz_name(view, rpz_obj, &new->passthru,
				    DNS_RPZ_PASSTHRU_ZONE, "zone");
	if (result != ISC_R_SUCCESS)
		return (result);

	obj = cfg_tuple_get(rpz_obj, "policy");
	if (cfg_obj_isvoid(obj)) {
		new->policy = DNS_RPZ_POLICY_GIVEN;
	} else {
		str = cfg_obj_asstring(cfg_tuple_get(obj, "policy name"));
		new->policy = dns_rpz_str2policy(str);
		INSIST(new->policy != DNS_RPZ_POLICY_ERROR);
		if (new->policy == DNS_RPZ_POLICY_CNAME) {
			str = cfg_obj_asstring(cfg_tuple_get(obj, "cname"));
			result = configure_rpz_name(view, rpz_obj, &new->cname,
						    str, "cname");
			if (result != ISC_R_SUCCESS)
				return (result);
		}
	}

	return (ISC_R_SUCCESS);
}

#ifdef USE_RRL
#define CHECK_RRL(cond, pat, val1, val2)				\
	do {								\
		if (!(cond)) {						\
			cfg_obj_log(obj, ns_g_lctx, ISC_LOG_ERROR,	\
				    pat, val1, val2);			\
			result = ISC_R_RANGE;				\
			goto cleanup;					\
		    }							\
	} while (0)

#define CHECK_RRL_RATE(rate, def, max_rate, name)			\
	do {								\
		obj = NULL;						\
		rrl->rate.str = name;					\
		result = cfg_map_get(map, name, &obj);			\
		if (result == ISC_R_SUCCESS) {				\
			rrl->rate.r = cfg_obj_asuint32(obj);		\
			CHECK_RRL(rrl->rate.r <= max_rate,		\
				  name" %d > %d",			\
				  rrl->rate.r, max_rate);		\
		} else {						\
			rrl->rate.r = def;				\
		}							\
		rrl->rate.scaled = rrl->rate.r;				\
	} while (0)

static isc_result_t
configure_rrl(dns_view_t *view, const cfg_obj_t *config, const cfg_obj_t *map) {
	const cfg_obj_t *obj;
	dns_rrl_t *rrl;
	isc_result_t result;
	int min_entries, i, j;

	/*
	 * Most DNS servers have few clients, but intentinally open
	 * recursive and authoritative servers often have many.
	 * So start with a small number of entries unless told otherwise
	 * to reduce cold-start costs.
	 */
	min_entries = 500;
	obj = NULL;
	result = cfg_map_get(map, "min-table-size", &obj);
	if (result == ISC_R_SUCCESS) {
		min_entries = cfg_obj_asuint32(obj);
		if (min_entries < 1)
			min_entries = 1;
	}
	result = dns_rrl_init(&rrl, view, min_entries);
	if (result != ISC_R_SUCCESS)
		return (result);

	i = ISC_MAX(20000, min_entries);
	obj = NULL;
	result = cfg_map_get(map, "max-table-size", &obj);
	if (result == ISC_R_SUCCESS) {
		i = cfg_obj_asuint32(obj);
		CHECK_RRL(i >= min_entries,
			  "max-table-size %d < min-table-size %d",
			  i, min_entries);
	}
	rrl->max_entries = i;

	CHECK_RRL_RATE(responses_per_second, 0, DNS_RRL_MAX_RATE,
		       "responses-per-second");
	CHECK_RRL_RATE(referrals_per_second,
		       rrl->responses_per_second.r, DNS_RRL_MAX_RATE,
		       "referrals-per-second");
	CHECK_RRL_RATE(nodata_per_second,
		       rrl->responses_per_second.r, DNS_RRL_MAX_RATE,
		       "nodata-per-second");
	CHECK_RRL_RATE(nxdomains_per_second,
		       rrl->responses_per_second.r, DNS_RRL_MAX_RATE,
		       "nxdomains-per-second");
	CHECK_RRL_RATE(errors_per_second,
		       rrl->responses_per_second.r, DNS_RRL_MAX_RATE,
		       "errors-per-second");

	CHECK_RRL_RATE(all_per_second, 0, DNS_RRL_MAX_RATE,
		       "all-per-second");

	CHECK_RRL_RATE(slip, 2, DNS_RRL_MAX_SLIP,
		       "slip");

	i = 15;
	obj = NULL;
	result = cfg_map_get(map, "window", &obj);
	if (result == ISC_R_SUCCESS) {
		i = cfg_obj_asuint32(obj);
		CHECK_RRL(i >= 1 && i <= DNS_RRL_MAX_WINDOW,
			  "window %d < 1 or > %d", i, DNS_RRL_MAX_WINDOW);
	}
	rrl->window = i;

	i = 0;
	obj = NULL;
	result = cfg_map_get(map, "qps-scale", &obj);
	if (result == ISC_R_SUCCESS) {
		i = cfg_obj_asuint32(obj);
		CHECK_RRL(i >= 1, "invalid 'qps-scale %d'%s", i, "");
	}
	rrl->qps_scale = i;
	rrl->qps = 1.0;

	i = 24;
	obj = NULL;
	result = cfg_map_get(map, "ipv4-prefix-length", &obj);
	if (result == ISC_R_SUCCESS) {
		i = cfg_obj_asuint32(obj);
		CHECK_RRL(i >= 8 && i <= 32,
			  "invalid 'ipv4-prefix-length %d'%s", i, "");
	}
	rrl->ipv4_prefixlen = i;
	if (i == 32)
		rrl->ipv4_mask = 0xffffffff;
	else
		rrl->ipv4_mask = htonl(0xffffffff << (32-i));

	i = 56;
	obj = NULL;
	result = cfg_map_get(map, "ipv6-prefix-length", &obj);
	if (result == ISC_R_SUCCESS) {
		i = cfg_obj_asuint32(obj);
		CHECK_RRL(i >= 16 && i <= DNS_RRL_MAX_PREFIX,
			  "ipv6-prefix-length %d < 16 or > %d",
			  i, DNS_RRL_MAX_PREFIX);
	}
	rrl->ipv6_prefixlen = i;
	for (j = 0; j < 4; ++j) {
		if (i <= 0) {
			rrl->ipv6_mask[j] = 0;
		} else if (i < 32) {
			rrl->ipv6_mask[j] = htonl(0xffffffff << (32-i));
		} else {
			rrl->ipv6_mask[j] = 0xffffffff;
		}
		i -= 32;
	}

	obj = NULL;
	result = cfg_map_get(map, "exempt-clients", &obj);
	if (result == ISC_R_SUCCESS) {
		result = cfg_acl_fromconfig(obj, config, ns_g_lctx,
					    ns_g_aclconfctx, ns_g_mctx,
					    0, &rrl->exempt);
		CHECK_RRL(result == ISC_R_SUCCESS,
			  "invalid %s%s", "address match list", "");
	}

	obj = NULL;
	result = cfg_map_get(map, "log-only", &obj);
	if (result == ISC_R_SUCCESS && cfg_obj_asboolean(obj))
		rrl->log_only = ISC_TRUE;
	else
		rrl->log_only = ISC_FALSE;

	return (ISC_R_SUCCESS);

 cleanup:
	dns_rrl_view_destroy(view);
	return (result);
}
#endif /* USE_RRL */

static isc_result_t
add_soa(dns_db_t *db, dns_dbversion_t *version, dns_name_t *name,
	dns_name_t *origin, dns_name_t *contact)
{
	dns_dbnode_t *node = NULL;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdatalist_t rdatalist;
	dns_rdataset_t rdataset;
	isc_result_t result;
	unsigned char buf[DNS_SOA_BUFFERSIZE];

	CHECK(dns_soa_buildrdata(origin, contact, dns_db_class(db),
				 0, 28800, 7200, 604800, 86400, buf, &rdata));

	dns_rdatalist_init(&rdatalist);
	rdatalist.type = rdata.type;
	rdatalist.rdclass = rdata.rdclass;
	rdatalist.ttl = 86400;
	ISC_LIST_APPEND(rdatalist.rdata, &rdata, link);

	dns_rdataset_init(&rdataset);
	CHECK(dns_rdatalist_tordataset(&rdatalist, &rdataset));
	CHECK(dns_db_findnode(db, name, ISC_TRUE, &node));
	CHECK(dns_db_addrdataset(db, node, version, 0, &rdataset, 0, NULL));

 cleanup:
	if (node != NULL)
		dns_db_detachnode(db, &node);
	return (result);
}

static isc_result_t
add_ns(dns_db_t *db, dns_dbversion_t *version, dns_name_t *name,
       dns_name_t *nsname)
{
	dns_dbnode_t *node = NULL;
	dns_rdata_ns_t ns;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdatalist_t rdatalist;
	dns_rdataset_t rdataset;
	isc_result_t result;
	isc_buffer_t b;
	unsigned char buf[DNS_NAME_MAXWIRE];

	isc_buffer_init(&b, buf, sizeof(buf));

	ns.common.rdtype = dns_rdatatype_ns;
	ns.common.rdclass = dns_db_class(db);
	ns.mctx = NULL;
	dns_name_init(&ns.name, NULL);
	dns_name_clone(nsname, &ns.name);
	CHECK(dns_rdata_fromstruct(&rdata, dns_db_class(db), dns_rdatatype_ns,
				   &ns, &b));

	dns_rdatalist_init(&rdatalist);
	rdatalist.type = rdata.type;
	rdatalist.rdclass = rdata.rdclass;
	rdatalist.ttl = 86400;
	ISC_LIST_APPEND(rdatalist.rdata, &rdata, link);

	dns_rdataset_init(&rdataset);
	CHECK(dns_rdatalist_tordataset(&rdatalist, &rdataset));
	CHECK(dns_db_findnode(db, name, ISC_TRUE, &node));
	CHECK(dns_db_addrdataset(db, node, version, 0, &rdataset, 0, NULL));

 cleanup:
	if (node != NULL)
		dns_db_detachnode(db, &node);
	return (result);
}

static isc_result_t
create_empty_zone(dns_zone_t *zone, dns_name_t *name, dns_view_t *view,
		  const cfg_obj_t *zonelist, const char **empty_dbtype,
		  int empty_dbtypec, dns_zonestat_level_t statlevel)
{
	char namebuf[DNS_NAME_FORMATSIZE];
	const cfg_listelt_t *element;
	const cfg_obj_t *obj;
	const cfg_obj_t *zconfig;
	const cfg_obj_t *zoptions;
	const char *rbt_dbtype[4] = { "rbt" };
	const char *sep = ": view ";
	const char *str;
	const char *viewname = view->name;
	dns_db_t *db = NULL;
	dns_dbversion_t *version = NULL;
	dns_fixedname_t cfixed;
	dns_fixedname_t fixed;
	dns_fixedname_t nsfixed;
	dns_name_t *contact;
	dns_name_t *ns;
	dns_name_t *zname;
	dns_zone_t *myzone = NULL;
	int rbt_dbtypec = 1;
	isc_result_t result;
	dns_namereln_t namereln;
	int order;
	unsigned int nlabels;

	dns_fixedname_init(&fixed);
	zname = dns_fixedname_name(&fixed);
	dns_fixedname_init(&nsfixed);
	ns = dns_fixedname_name(&nsfixed);
	dns_fixedname_init(&cfixed);
	contact = dns_fixedname_name(&cfixed);

	/*
	 * Look for forward "zones" beneath this empty zone and if so
	 * create a custom db for the empty zone.
	 */
	for (element = cfg_list_first(zonelist);
	     element != NULL;
	     element = cfg_list_next(element)) {

		zconfig = cfg_listelt_value(element);
		str = cfg_obj_asstring(cfg_tuple_get(zconfig, "name"));
		CHECK(dns_name_fromstring(zname, str, 0, NULL));
		namereln = dns_name_fullcompare(zname, name, &order, &nlabels);
		if (namereln != dns_namereln_subdomain)
			continue;

		zoptions = cfg_tuple_get(zconfig, "options");

		obj = NULL;
		(void)cfg_map_get(zoptions, "type", &obj);
		INSIST(obj != NULL);
		if (strcasecmp(cfg_obj_asstring(obj), "forward") == 0) {
			obj = NULL;
			(void)cfg_map_get(zoptions, "forward", &obj);
			if (obj == NULL)
				continue;
			if (strcasecmp(cfg_obj_asstring(obj), "only") != 0)
				continue;
		}
		if (db == NULL) {
			CHECK(dns_db_create(view->mctx, "rbt", name,
					    dns_dbtype_zone, view->rdclass,
					    0, NULL, &db));
			CHECK(dns_db_newversion(db, &version));
			if (strcmp(empty_dbtype[2], "@") == 0)
				dns_name_clone(name, ns);
			else
				CHECK(dns_name_fromstring(ns, empty_dbtype[2],
							  0, NULL));
			CHECK(dns_name_fromstring(contact, empty_dbtype[3],
						  0, NULL));
			CHECK(add_soa(db, version, name, ns, contact));
			CHECK(add_ns(db, version, name, ns));
		}
		CHECK(add_ns(db, version, zname, dns_rootname));
	}

	/*
	 * Is the existing zone the ok to use?
	 */
	if (zone != NULL) {
		unsigned int typec;
		const char **dbargv;

		if (db != NULL) {
			typec = rbt_dbtypec;
			dbargv = rbt_dbtype;
		} else {
			typec = empty_dbtypec;
			dbargv = empty_dbtype;
		}

		result = check_dbtype(zone, typec, dbargv, view->mctx);
		if (result != ISC_R_SUCCESS)
			zone = NULL;

		if (zone != NULL && dns_zone_gettype(zone) != dns_zone_master)
			zone = NULL;
		if (zone != NULL && dns_zone_getfile(zone) != NULL)
			zone = NULL;
		if (zone != NULL) {
			dns_zone_getraw(zone, &myzone);
			if (myzone != NULL) {
				dns_zone_detach(&myzone);
				zone = NULL;
			}
		}
	}

	if (zone == NULL) {
		CHECK(dns_zonemgr_createzone(ns_g_server->zonemgr, &myzone));
		zone = myzone;
		CHECK(dns_zone_setorigin(zone, name));
		CHECK(dns_zonemgr_managezone(ns_g_server->zonemgr, zone));
		if (db == NULL)
			CHECK(dns_zone_setdbtype(zone, empty_dbtypec,
						 empty_dbtype));
		dns_zone_setclass(zone, view->rdclass);
		dns_zone_settype(zone, dns_zone_master);
		dns_zone_setstats(zone, ns_g_server->zonestats);
	}

	dns_zone_setoption(zone, ~DNS_ZONEOPT_NOCHECKNS, ISC_FALSE);
	dns_zone_setoption(zone, DNS_ZONEOPT_NOCHECKNS, ISC_TRUE);
	dns_zone_setnotifytype(zone, dns_notifytype_no);
	dns_zone_setdialup(zone, dns_dialuptype_no);
	if (view->queryacl != NULL)
		dns_zone_setqueryacl(zone, view->queryacl);
	else
		dns_zone_clearqueryacl(zone);
	if (view->queryonacl != NULL)
		dns_zone_setqueryonacl(zone, view->queryonacl);
	else
		dns_zone_clearqueryonacl(zone);
	dns_zone_clearupdateacl(zone);
	if (view->transferacl != NULL)
		dns_zone_setxfracl(zone, view->transferacl);
	else
		dns_zone_clearxfracl(zone);

	CHECK(setquerystats(zone, view->mctx, statlevel));
	if (db != NULL) {
		dns_db_closeversion(db, &version, ISC_TRUE);
		CHECK(dns_zone_replacedb(zone, db, ISC_FALSE));
	}
	dns_zone_setview(zone, view);
	CHECK(dns_view_addzone(view, zone));

	if (!strcmp(viewname, "_default")) {
		sep = "";
		viewname = "";
	}
	dns_name_format(name, namebuf, sizeof(namebuf));
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      ISC_LOG_INFO, "automatic empty zone%s%s: %s",
		      sep, viewname, namebuf);

 cleanup:
	if (myzone != NULL)
		dns_zone_detach(&myzone);
	if (version != NULL)
		dns_db_closeversion(db, &version, ISC_FALSE);
	if (db != NULL)
		dns_db_detach(&db);

	INSIST(version == NULL);

	return (result);
}

/*
 * Configure 'view' according to 'vconfig', taking defaults from 'config'
 * where values are missing in 'vconfig'.
 *
 * When configuring the default view, 'vconfig' will be NULL and the
 * global defaults in 'config' used exclusively.
 */
static isc_result_t
configure_view(dns_view_t *view, cfg_obj_t *config, cfg_obj_t *vconfig,
	       ns_cachelist_t *cachelist, const cfg_obj_t *bindkeys,
	       isc_mem_t *mctx, cfg_aclconfctx_t *actx,
	       isc_boolean_t need_hints)
{
	const cfg_obj_t *maps[4];
	const cfg_obj_t *cfgmaps[3];
	const cfg_obj_t *optionmaps[3];
	const cfg_obj_t *options = NULL;
	const cfg_obj_t *voptions = NULL;
	const cfg_obj_t *forwardtype;
	const cfg_obj_t *forwarders;
	const cfg_obj_t *alternates;
	const cfg_obj_t *zonelist;
	const cfg_obj_t *dlz;
	unsigned int dlzargc;
	char **dlzargv;
	const cfg_obj_t *disabled;
	const cfg_obj_t *obj;
#ifdef ENABLE_FETCHLIMIT
	const cfg_obj_t *obj2;
#endif /* ENABLE_FETCHLIMIT */
	const cfg_listelt_t *element;
	in_port_t port;
	dns_cache_t *cache = NULL;
	isc_result_t result;
	unsigned int cleaning_interval;
	size_t max_cache_size;
	size_t max_acache_size;
	size_t max_adb_size;
	isc_uint32_t lame_ttl;
	dns_tsig_keyring_t *ring = NULL;
	dns_view_t *pview = NULL;	/* Production view */
	isc_mem_t *cmctx = NULL, *hmctx = NULL;
	dns_dispatch_t *dispatch4 = NULL;
	dns_dispatch_t *dispatch6 = NULL;
	isc_boolean_t reused_cache = ISC_FALSE;
	isc_boolean_t shared_cache = ISC_FALSE;
	int i = 0, j = 0, k = 0;
	const char *str;
	const char *cachename = NULL;
	dns_order_t *order = NULL;
	isc_uint32_t udpsize;
	isc_uint32_t maxbits;
	unsigned int resopts = 0;
	dns_zone_t *zone = NULL;
	isc_uint32_t max_clients_per_query;
	isc_boolean_t empty_zones_enable;
	const cfg_obj_t *disablelist = NULL;
	isc_stats_t *resstats = NULL;
	dns_stats_t *resquerystats = NULL;
	isc_boolean_t auto_dlv = ISC_FALSE;
	isc_boolean_t auto_root = ISC_FALSE;
	ns_cache_t *nsc;
	isc_boolean_t zero_no_soattl;
	dns_acl_t *clients = NULL, *mapped = NULL, *excluded = NULL;
	unsigned int query_timeout, ndisp;
	struct cfg_context *nzctx;
	dns_rpz_zone_t *rpz;

	REQUIRE(DNS_VIEW_VALID(view));

	if (config != NULL)
		(void)cfg_map_get(config, "options", &options);

	/*
	 * maps: view options, options, defaults
	 * cfgmaps: view options, config
	 * optionmaps: view options, options
	 */
	if (vconfig != NULL) {
		voptions = cfg_tuple_get(vconfig, "options");
		maps[i++] = voptions;
		optionmaps[j++] = voptions;
		cfgmaps[k++] = voptions;
	}
	if (options != NULL) {
		maps[i++] = options;
		optionmaps[j++] = options;
	}

	maps[i++] = ns_g_defaults;
	maps[i] = NULL;
	optionmaps[j] = NULL;
	if (config != NULL)
		cfgmaps[k++] = config;
	cfgmaps[k] = NULL;

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
			if (value > SIZE_MAX) {
				cfg_obj_log(obj, ns_g_lctx,
					    ISC_LOG_WARNING,
					    "'max-acache-size "
					    "%" ISC_PRINT_QUADFORMAT "u' "
					    "is too large for this "
					    "system; reducing to %lu",
					    value, (unsigned long)SIZE_MAX);
				value = SIZE_MAX;
			}
			max_acache_size = (size_t) value;
		}
		dns_acache_setcachesize(view->acache, max_acache_size);
	}

	CHECK(configure_view_acl(vconfig, config, "allow-query", NULL, actx,
				 ns_g_mctx, &view->queryacl));
	if (view->queryacl == NULL) {
		CHECK(configure_view_acl(NULL, ns_g_config, "allow-query",
					 NULL, actx, ns_g_mctx,
					 &view->queryacl));
	}

	/*
	 * Make the list of response policy zone names for a view that
	 * is used for real lookups and so cares about hints.
	 */
	obj = NULL;
	if (view->rdclass == dns_rdataclass_in && need_hints &&
	    ns_config_get(maps, "response-policy", &obj) == ISC_R_SUCCESS) {
		const cfg_obj_t *rpz_obj;
		isc_boolean_t recursive_only_def;
		dns_ttl_t ttl_def;

		rpz_obj = cfg_tuple_get(obj, "recursive-only");
		if (!cfg_obj_isvoid(rpz_obj) &&
		    !cfg_obj_asboolean(rpz_obj))
			recursive_only_def = ISC_FALSE;
		else
			recursive_only_def = ISC_TRUE;

		rpz_obj = cfg_tuple_get(obj, "break-dnssec");
		if (!cfg_obj_isvoid(rpz_obj) &&
		    cfg_obj_asboolean(rpz_obj))
			view->rpz_break_dnssec = ISC_TRUE;
		else
			view->rpz_break_dnssec = ISC_FALSE;

		rpz_obj = cfg_tuple_get(obj, "max-policy-ttl");
		if (cfg_obj_isuint32(rpz_obj))
			ttl_def = cfg_obj_asuint32(rpz_obj);
		else
			ttl_def = DNS_RPZ_MAX_TTL_DEFAULT;

		rpz_obj = cfg_tuple_get(obj, "min-ns-dots");
		if (cfg_obj_isuint32(rpz_obj))
			view->rpz_min_ns_labels = cfg_obj_asuint32(rpz_obj) + 1;
		else
			view->rpz_min_ns_labels = 2;

		element = cfg_list_first(cfg_tuple_get(obj, "zone list"));
		while (element != NULL) {
			result = configure_rpz(view, element,
					       recursive_only_def, ttl_def);
			if (result != ISC_R_SUCCESS)
				goto cleanup;
			element = cfg_list_next(element);
		}
	}

	/*
	 * Configure the zones.
	 */
	zonelist = NULL;
	if (voptions != NULL)
		(void)cfg_map_get(voptions, "zone", &zonelist);
	else
		(void)cfg_map_get(config, "zone", &zonelist);

	/*
	 * Load zone configuration
	 */
	for (element = cfg_list_first(zonelist);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const cfg_obj_t *zconfig = cfg_listelt_value(element);
		CHECK(configure_zone(config, zconfig, vconfig, mctx, view,
				     actx, ISC_FALSE));
	}

	for (rpz = ISC_LIST_HEAD(view->rpz_zones);
	     rpz != NULL;
	     rpz = ISC_LIST_NEXT(rpz, link))
	{
		if (!rpz->defined) {
			char namebuf[DNS_NAME_FORMATSIZE];

			dns_name_format(&rpz->origin, namebuf, sizeof(namebuf));
			cfg_obj_log(obj, ns_g_lctx, DNS_RPZ_ERROR_LEVEL,
				    "'%s' is not a master or slave zone",
				    namebuf);
			result = ISC_R_NOTFOUND;
			goto cleanup;
		}
	}

	/*
	 * If we're allowing added zones, then load zone configuration
	 * from the newzone file for zones that were added during previous
	 * runs.
	 */
	nzctx = view->new_zone_config;
	if (nzctx != NULL && nzctx->nzconfig != NULL) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
			      "loading additional zones for view '%s'",
			      view->name);

		zonelist = NULL;
		cfg_map_get(nzctx->nzconfig, "zone", &zonelist);

		for (element = cfg_list_first(zonelist);
		     element != NULL;
		     element = cfg_list_next(element))
		{
			const cfg_obj_t *zconfig = cfg_listelt_value(element);
			CHECK(configure_zone(config, zconfig, vconfig,
					     mctx, view, actx,
					     ISC_TRUE));
		}
	}

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
		(void)cfg_map_get(dlz, "database", &obj);
		if (obj != NULL) {
			const cfg_obj_t *name;
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

			name = cfg_map_getname(dlz);
			result = dns_dlzcreate(mctx, cfg_obj_asstring(name),
					       dlzargv[0], dlzargc, dlzargv,
					       &view->dlzdatabase);
			isc_mem_free(mctx, s);
			isc_mem_put(mctx, dlzargv, dlzargc * sizeof(*dlzargv));
			if (result != ISC_R_SUCCESS)
				goto cleanup;

			/*
			 * If the dlz backend supports configuration,
			 * then call its configure method now.
			 */
			result = dns_dlzconfigure(view, dlzconfigure_callback);
			if (result != ISC_R_SUCCESS)
				goto cleanup;
		}
	}

	/*
	 * Obtain configuration parameters that affect the decision of whether
	 * we can reuse/share an existing cache.
	 */
	obj = NULL;
	result = ns_config_get(maps, "cleaning-interval", &obj);
	INSIST(result == ISC_R_SUCCESS);
	cleaning_interval = cfg_obj_asuint32(obj) * 60;

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
		if (value > SIZE_MAX) {
			cfg_obj_log(obj, ns_g_lctx,
				    ISC_LOG_WARNING,
				    "'max-cache-size "
				    "%" ISC_PRINT_QUADFORMAT "u' "
				    "is too large for this "
				    "system; reducing to %lu",
				    value, (unsigned long)SIZE_MAX);
			value = SIZE_MAX;
		}
		max_cache_size = (size_t) value;
	}

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
	result = ns_config_get(maps, "dns64", &obj);
	if (result == ISC_R_SUCCESS && strcmp(view->name, "_bind") &&
	    strcmp(view->name, "_meta")) {
		isc_netaddr_t na, suffix, *sp;
		unsigned int prefixlen;
		const char *server, *contact;
		const cfg_obj_t *myobj;

		myobj = NULL;
		result = ns_config_get(maps, "dns64-server", &myobj);
		if (result == ISC_R_SUCCESS)
			server = cfg_obj_asstring(myobj);
		else
			server = NULL;

		myobj = NULL;
		result = ns_config_get(maps, "dns64-contact", &myobj);
		if (result == ISC_R_SUCCESS)
			contact = cfg_obj_asstring(myobj);
		else
			contact = NULL;

		for (element = cfg_list_first(obj);
		     element != NULL;
		     element = cfg_list_next(element))
		{
			const cfg_obj_t *map = cfg_listelt_value(element);
			dns_dns64_t *dns64 = NULL;
			unsigned int dns64options = 0;

			cfg_obj_asnetprefix(cfg_map_getname(map), &na,
					    &prefixlen);

			obj = NULL;
			(void)cfg_map_get(map, "suffix", &obj);
			if (obj != NULL) {
				sp = &suffix;
				isc_netaddr_fromsockaddr(sp,
						      cfg_obj_assockaddr(obj));
			} else
				sp = NULL;

			clients = mapped = excluded = NULL;
			obj = NULL;
			(void)cfg_map_get(map, "clients", &obj);
			if (obj != NULL) {
				result = cfg_acl_fromconfig(obj, config,
							    ns_g_lctx, actx,
							    mctx, 0, &clients);
				if (result != ISC_R_SUCCESS)
					goto cleanup;
			}
			obj = NULL;
			(void)cfg_map_get(map, "mapped", &obj);
			if (obj != NULL) {
				result = cfg_acl_fromconfig(obj, config,
							    ns_g_lctx, actx,
							    mctx, 0, &mapped);
				if (result != ISC_R_SUCCESS)
					goto cleanup;
			}
			obj = NULL;
			(void)cfg_map_get(map, "exclude", &obj);
			if (obj != NULL) {
				result = cfg_acl_fromconfig(obj, config,
							    ns_g_lctx, actx,
							    mctx, 0, &excluded);
				if (result != ISC_R_SUCCESS)
					goto cleanup;
			}

			obj = NULL;
			(void)cfg_map_get(map, "recursive-only", &obj);
			if (obj != NULL && cfg_obj_asboolean(obj))
				dns64options |= DNS_DNS64_RECURSIVE_ONLY;

			obj = NULL;
			(void)cfg_map_get(map, "break-dnssec", &obj);
			if (obj != NULL && cfg_obj_asboolean(obj))
				dns64options |= DNS_DNS64_BREAK_DNSSEC;

			result = dns_dns64_create(mctx, &na, prefixlen, sp,
						  clients, mapped, excluded,
						  dns64options, &dns64);
			if (result != ISC_R_SUCCESS)
				goto cleanup;
			dns_dns64_append(&view->dns64, dns64);
			view->dns64cnt++;
			result = dns64_reverse(view, mctx, &na, prefixlen,
					       server, contact);
			if (result != ISC_R_SUCCESS)
				goto cleanup;
			if (clients != NULL)
				dns_acl_detach(&clients);
			if (mapped != NULL)
				dns_acl_detach(&mapped);
			if (excluded != NULL)
				dns_acl_detach(&excluded);
		}
	}

	obj = NULL;
	result = ns_config_get(maps, "dnssec-accept-expired", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->acceptexpired = cfg_obj_asboolean(obj);

	obj = NULL;
	result = ns_config_get(maps, "dnssec-validation", &obj);
	INSIST(result == ISC_R_SUCCESS);
	if (cfg_obj_isboolean(obj)) {
		view->enablevalidation = cfg_obj_asboolean(obj);
	} else {
		/* If dnssec-validation is not boolean, it must be "auto" */
		view->enablevalidation = ISC_TRUE;
		auto_root = ISC_TRUE;
	}

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
	 * Configure the view's cache.
	 *
	 * First, check to see if there are any attach-cache options.  If yes,
	 * attempt to lookup an existing cache at attach it to the view.  If
	 * there is not one, then try to reuse an existing cache if possible;
	 * otherwise create a new cache.
	 *
	 * Note that the ADB is not preserved or shared in either case.
	 *
	 * When a matching view is found, the associated statistics are also
	 * retrieved and reused.
	 *
	 * XXX Determining when it is safe to reuse or share a cache is tricky.
	 * When the view's configuration changes, the cached data may become
	 * invalid because it reflects our old view of the world.  We check
	 * some of the configuration parameters that could invalidate the cache
	 * or otherwise make it unsharable, but there are other configuration
	 * options that should be checked.  For example, if a view uses a
	 * forwarder, changes in the forwarder configuration may invalidate
	 * the cache.  At the moment, it's the administrator's responsibility to
	 * ensure these configuration options don't invalidate reusing/sharing.
	 */
	obj = NULL;
	result = ns_config_get(maps, "attach-cache", &obj);
	if (result == ISC_R_SUCCESS)
		cachename = cfg_obj_asstring(obj);
	else
		cachename = view->name;
	cache = NULL;
	nsc = cachelist_find(cachelist, cachename);
	if (nsc != NULL) {
		if (!cache_sharable(nsc->primaryview, view, zero_no_soattl,
				    cleaning_interval, max_cache_size)) {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
				      "views %s and %s can't share the cache "
				      "due to configuration parameter mismatch",
				      nsc->primaryview->name, view->name);
			result = ISC_R_FAILURE;
			goto cleanup;
		}
		dns_cache_attach(nsc->cache, &cache);
		shared_cache = ISC_TRUE;
	} else {
		if (strcmp(cachename, view->name) == 0) {
			result = dns_viewlist_find(&ns_g_server->viewlist,
						   cachename, view->rdclass,
						   &pview);
			if (result != ISC_R_NOTFOUND && result != ISC_R_SUCCESS)
				goto cleanup;
			if (pview != NULL) {
				if (!cache_reusable(pview, view,
						    zero_no_soattl)) {
					isc_log_write(ns_g_lctx,
						      NS_LOGCATEGORY_GENERAL,
						      NS_LOGMODULE_SERVER,
						      ISC_LOG_DEBUG(1),
						      "cache cannot be reused "
						      "for view %s due to "
						      "configuration parameter "
						      "mismatch", view->name);
				} else {
					INSIST(pview->cache != NULL);
					isc_log_write(ns_g_lctx,
						      NS_LOGCATEGORY_GENERAL,
						      NS_LOGMODULE_SERVER,
						      ISC_LOG_DEBUG(3),
						      "reusing existing cache");
					reused_cache = ISC_TRUE;
					dns_cache_attach(pview->cache, &cache);
				}
				dns_view_getresstats(pview, &resstats);
				dns_view_getresquerystats(pview,
							  &resquerystats);
				dns_view_detach(&pview);
			}
		}
		if (cache == NULL) {
			/*
			 * Create a cache with the desired name.  This normally
			 * equals the view name, but may also be a forward
			 * reference to a view that share the cache with this
			 * view but is not yet configured.  If it is not the
			 * view name but not a forward reference either, then it
			 * is simply a named cache that is not shared.
			 *
			 * We use two separate memory contexts for the
			 * cache, for the main cache memory and the heap
			 * memory.
			 */
			CHECK(isc_mem_create(0, 0, &cmctx));
			isc_mem_setname(cmctx, "cache", NULL);
			CHECK(isc_mem_create(0, 0, &hmctx));
			isc_mem_setname(hmctx, "cache_heap", NULL);
			CHECK(dns_cache_create3(cmctx, hmctx, ns_g_taskmgr,
						ns_g_timermgr, view->rdclass,
						cachename, "rbt", 0, NULL,
						&cache));
			isc_mem_detach(&cmctx);
			isc_mem_detach(&hmctx);
		}
		nsc = isc_mem_get(mctx, sizeof(*nsc));
		if (nsc == NULL) {
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
		nsc->cache = NULL;
		dns_cache_attach(cache, &nsc->cache);
		nsc->primaryview = view;
		nsc->needflush = ISC_FALSE;
		nsc->adbsizeadjusted = ISC_FALSE;
		ISC_LINK_INIT(nsc, link);
		ISC_LIST_APPEND(*cachelist, nsc, link);
	}
	dns_view_setcache2(view, cache, shared_cache);

	/*
	 * cache-file cannot be inherited if views are present, but this
	 * should be caught by the configuration checking stage.
	 */
	obj = NULL;
	result = ns_config_get(maps, "cache-file", &obj);
	if (result == ISC_R_SUCCESS && strcmp(view->name, "_bind") != 0) {
		CHECK(dns_cache_setfilename(cache, cfg_obj_asstring(obj)));
		if (!reused_cache && !shared_cache)
			CHECK(dns_cache_load(cache));
	}

	dns_cache_setcleaninginterval(cache, cleaning_interval);
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

	ndisp = 4 * ISC_MIN(ns_g_udpdisp, MAX_UDP_DISPATCH);
	CHECK(dns_view_createresolver(view, ns_g_taskmgr, 31, ndisp,
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
	 * Set the ADB cache size to 1/8th of the max-cache-size or
	 * MAX_ADB_SIZE_FOR_CACHESHARE when the cache is shared.
	 */
	max_adb_size = 0;
	if (max_cache_size != 0U) {
		max_adb_size = max_cache_size / 8;
		if (max_adb_size == 0U)
			max_adb_size = 1;	/* Force minimum. */
		if (view != nsc->primaryview &&
		    max_adb_size > MAX_ADB_SIZE_FOR_CACHESHARE) {
			max_adb_size = MAX_ADB_SIZE_FOR_CACHESHARE;
			if (!nsc->adbsizeadjusted) {
				dns_adb_setadbsize(nsc->primaryview->adb,
						   MAX_ADB_SIZE_FOR_CACHESHARE);
				nsc->adbsizeadjusted = ISC_TRUE;
			}
		}
	}
	dns_adb_setadbsize(view->adb, max_adb_size);

#ifdef ENABLE_FETCHLIMIT
	/*
	 * Set up ADB quotas
	 */
	{
		isc_uint32_t fps, freq;
		double low, high, discount;

		obj = NULL;
		result = ns_config_get(maps, "fetches-per-server", &obj);
		INSIST(result == ISC_R_SUCCESS);
		obj2 = cfg_tuple_get(obj, "fetches");
		fps = cfg_obj_asuint32(obj2);
		obj2 = cfg_tuple_get(obj, "response");
		if (!cfg_obj_isvoid(obj2)) {
			const char *resp = cfg_obj_asstring(obj2);
			isc_result_t r;

			if (strcasecmp(resp, "drop") == 0)
				r = DNS_R_DROP;
			else if (strcasecmp(resp, "fail") == 0)
				r = DNS_R_SERVFAIL;
			else
				INSIST(0);

			dns_resolver_setquotaresponse(view->resolver,
						      dns_quotatype_server, r);
		}

		obj = NULL;
		result = ns_config_get(maps, "fetch-quota-params", &obj);
		INSIST(result == ISC_R_SUCCESS);

		obj2 = cfg_tuple_get(obj, "frequency");
		freq = cfg_obj_asuint32(obj2);

		obj2 = cfg_tuple_get(obj, "low");
		low = (double) cfg_obj_asfixedpoint(obj2) / 100.0;

		obj2 = cfg_tuple_get(obj, "high");
		high = (double) cfg_obj_asfixedpoint(obj2) / 100.0;

		obj2 = cfg_tuple_get(obj, "discount");
		discount = (double) cfg_obj_asfixedpoint(obj2) / 100.0;

		dns_adb_setquota(view->adb, fps, freq, low, high, discount);
	}
#endif /* ENABLE_FETCHLIMIT */

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
	 * Set the resolver's query timeout.
	 */
	obj = NULL;
	result = ns_config_get(maps, "resolver-query-timeout", &obj);
	INSIST(result == ISC_R_SUCCESS);
	query_timeout = cfg_obj_asuint32(obj);
	dns_resolver_settimeout(view->resolver, query_timeout);

	/* Specify whether to use 0-TTL for negative response for SOA query */
	dns_resolver_setzeronosoattl(view->resolver, zero_no_soattl);

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
	 * Set the maximum rsa exponent bits.
	 */
	obj = NULL;
	result = ns_config_get(maps, "max-rsa-exponent-size", &obj);
	INSIST(result == ISC_R_SUCCESS);
	maxbits = cfg_obj_asuint32(obj);
	if (maxbits != 0 && maxbits < 35)
		maxbits = 35;
	if (maxbits > 4096)
		maxbits = 4096;
	view->maxbits = maxbits;

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
	CHECK(ns_tsigkeyring_fromconfig(config, vconfig, view->mctx, &ring));
	if (ns_g_server->sessionkey != NULL) {
		CHECK(dns_tsigkeyring_add(ring, ns_g_server->session_keyname,
					  ns_g_server->sessionkey));
	}
	dns_view_setkeyring(view, ring);
	dns_tsigkeyring_detach(&ring);

	/*
	 * See if we can re-use a dynamic key ring.
	 */
	result = dns_viewlist_find(&ns_g_server->viewlist, view->name,
				   view->rdclass, &pview);
	if (result != ISC_R_NOTFOUND && result != ISC_R_SUCCESS)
		goto cleanup;
	if (pview != NULL) {
		dns_view_getdynamickeyring(pview, &ring);
		if (ring != NULL)
			dns_view_setdynamickeyring(view, ring);
		dns_tsigkeyring_detach(&ring);
		dns_view_detach(&pview);
	} else
		dns_view_restorekeyring(view);

	/*
	 * Configure the view's peer list.
	 */
	{
		const cfg_obj_t *peers = NULL;
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
	CHECK(configure_view_acl(vconfig, config, "match-clients", NULL, actx,
				 ns_g_mctx, &view->matchclients));
	CHECK(configure_view_acl(vconfig, config, "match-destinations", NULL,
				 actx, ns_g_mctx, &view->matchdestinations));

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
	CHECK(configure_view_acl(vconfig, config, "allow-query-cache", NULL,
				 actx, ns_g_mctx, &view->cacheacl));
	CHECK(configure_view_acl(vconfig, config, "allow-query-cache-on", NULL,
				 actx, ns_g_mctx, &view->cacheonacl));
	if (view->cacheonacl == NULL)
		CHECK(configure_view_acl(NULL, ns_g_config,
					 "allow-query-cache-on", NULL, actx,
					 ns_g_mctx, &view->cacheonacl));
	if (strcmp(view->name, "_bind") != 0) {
		CHECK(configure_view_acl(vconfig, config, "allow-recursion",
					 NULL, actx, ns_g_mctx,
					 &view->recursionacl));
		CHECK(configure_view_acl(vconfig, config, "allow-recursion-on",
					 NULL, actx, ns_g_mctx,
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
	/*
	 * XXXEACH: This call to configure_view_acl() is redundant.  We
	 * are leaving it as it is because we are making a minimal change
	 * for a patch release.  In the future this should be changed to
	 * dns_acl_attach(view->queryacl, &view->cacheacl).
	 */
	if (view->cacheacl == NULL && view->recursion)
		CHECK(configure_view_acl(vconfig, config, "allow-query", NULL,
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
					 "allow-recursion", NULL,
					 actx, ns_g_mctx,
					 &view->recursionacl));
	if (view->recursiononacl == NULL && view->recursion)
		CHECK(configure_view_acl(NULL, ns_g_config,
					 "allow-recursion-on", NULL,
					 actx, ns_g_mctx,
					 &view->recursiononacl));
	if (view->cacheacl == NULL) {
		if (view->recursion)
			CHECK(configure_view_acl(NULL, ns_g_config,
						 "allow-query-cache", NULL,
						 actx, ns_g_mctx,
						 &view->cacheacl));
		else
			CHECK(dns_acl_none(mctx, &view->cacheacl));
	}

	/*
	 * Ignore case when compressing responses to the specified
	 * clients. This causes case not always to be preserved,
	 * and is needed by some broken clients.
	 */
	CHECK(configure_view_acl(vconfig, config, "no-case-compress", NULL,
				 actx, ns_g_mctx, &view->nocasecompress));

	/*
	 * Filter setting on addresses in the answer section.
	 */
	CHECK(configure_view_acl(vconfig, config, "deny-answer-addresses",
				 "acl", actx, ns_g_mctx, &view->denyansweracl));
	CHECK(configure_view_nametable(vconfig, config, "deny-answer-addresses",
				       "except-from", ns_g_mctx,
				       &view->answeracl_exclude));

	/*
	 * Filter setting on names (CNAME/DNAME targets) in the answer section.
	 */
	CHECK(configure_view_nametable(vconfig, config, "deny-answer-aliases",
				       "name", ns_g_mctx,
				       &view->denyanswernames));
	CHECK(configure_view_nametable(vconfig, config, "deny-answer-aliases",
				       "except-from", ns_g_mctx,
				       &view->answernames_exclude));

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
					 "allow-notify", NULL, actx,
					 ns_g_mctx, &view->notifyacl));
	if (view->transferacl == NULL)
		CHECK(configure_view_acl(NULL, ns_g_config,
					 "allow-transfer", NULL, actx,
					 ns_g_mctx, &view->transferacl));
	if (view->updateacl == NULL)
		CHECK(configure_view_acl(NULL, ns_g_config,
					 "allow-update", NULL, actx,
					 ns_g_mctx, &view->updateacl));
	if (view->upfwdacl == NULL)
		CHECK(configure_view_acl(NULL, ns_g_config,
					 "allow-update-forwarding", NULL, actx,
					 ns_g_mctx, &view->upfwdacl));

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
	result = ns_config_get(maps, "max-recursion-depth", &obj);
	INSIST(result == ISC_R_SUCCESS);
	dns_resolver_setmaxdepth(view->resolver, cfg_obj_asuint32(obj));

	obj = NULL;
	result = ns_config_get(maps, "max-recursion-queries", &obj);
	INSIST(result == ISC_R_SUCCESS);
	dns_resolver_setmaxqueries(view->resolver, cfg_obj_asuint32(obj));

#ifdef ENABLE_FETCHLIMIT
	obj = NULL;
	result = ns_config_get(maps, "fetches-per-zone", &obj);
	INSIST(result == ISC_R_SUCCESS);
	obj2 = cfg_tuple_get(obj, "fetches");
	dns_resolver_setfetchesperzone(view->resolver, cfg_obj_asuint32(obj2));
	obj2 = cfg_tuple_get(obj, "response");
	if (!cfg_obj_isvoid(obj2)) {
		const char *resp = cfg_obj_asstring(obj2);
		isc_result_t r;

		if (strcasecmp(resp, "drop") == 0)
			r = DNS_R_DROP;
		else if (strcasecmp(resp, "fail") == 0)
			r = DNS_R_SERVFAIL;
		else
			INSIST(0);

		dns_resolver_setquotaresponse(view->resolver,
					      dns_quotatype_zone, r);
	}
#endif /* ENABLE_FETCHLIMIT */

#ifdef ALLOW_FILTER_AAAA_ON_V4
	obj = NULL;
	result = ns_config_get(maps, "filter-aaaa-on-v4", &obj);
	INSIST(result == ISC_R_SUCCESS);
	if (cfg_obj_isboolean(obj)) {
		if (cfg_obj_asboolean(obj))
			view->v4_aaaa = dns_v4_aaaa_filter;
		else
			view->v4_aaaa = dns_v4_aaaa_ok;
	} else {
		const char *v4_aaaastr = cfg_obj_asstring(obj);
		if (strcasecmp(v4_aaaastr, "break-dnssec") == 0)
			view->v4_aaaa = dns_v4_aaaa_break_dnssec;
		else
			INSIST(0);
	}
	CHECK(configure_view_acl(vconfig, config, "filter-aaaa", NULL,
				 actx, ns_g_mctx, &view->v4_aaaa_acl));
#endif

	obj = NULL;
	result = ns_config_get(maps, "dnssec-enable", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->enablednssec = cfg_obj_asboolean(obj);

	obj = NULL;
	result = ns_config_get(optionmaps, "dnssec-lookaside", &obj);
	if (result == ISC_R_SUCCESS) {
		/* If set to "auto", use the version from the defaults */
		const cfg_obj_t *dlvobj;
		const char *dom;
		dlvobj = cfg_listelt_value(cfg_list_first(obj));
		dom = cfg_obj_asstring(cfg_tuple_get(dlvobj, "domain"));
		if (cfg_obj_isvoid(cfg_tuple_get(dlvobj, "trust-anchor"))) {
			/* If "no", skip; if "auto", use global default */
			if (!strcasecmp(dom, "no"))
				result = ISC_R_NOTFOUND;
			else if (!strcasecmp(dom, "auto")) {
				auto_dlv = ISC_TRUE;
				obj = NULL;
				result = cfg_map_get(ns_g_defaults,
						     "dnssec-lookaside", &obj);
			}
		}
	}

	if (result == ISC_R_SUCCESS) {
		for (element = cfg_list_first(obj);
		     element != NULL;
		     element = cfg_list_next(element))
		{
			dns_name_t *dlv;

			obj = cfg_listelt_value(element);
			obj = cfg_tuple_get(obj, "trust-anchor");
			dlv = dns_fixedname_name(&view->dlv_fixed);
			CHECK(dns_name_fromstring(dlv, cfg_obj_asstring(obj),
						  DNS_NAME_DOWNCASE, NULL));
			view->dlv = dns_fixedname_name(&view->dlv_fixed);
		}
	} else
		view->dlv = NULL;

	/*
	 * For now, there is only one kind of trusted keys, the
	 * "security roots".
	 */
	CHECK(configure_view_dnsseckeys(view, vconfig, config, bindkeys,
					auto_dlv, auto_root, mctx));
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
	if (result == ISC_R_SUCCESS)
		dns_view_setrootdelonly(view, ISC_TRUE);
	if (result == ISC_R_SUCCESS && ! cfg_obj_isvoid(obj)) {
		const cfg_obj_t *exclude;
		dns_fixedname_t fixed;
		dns_name_t *name;

		dns_fixedname_init(&fixed);
		name = dns_fixedname_name(&fixed);
		for (element = cfg_list_first(obj);
		     element != NULL;
		     element = cfg_list_next(element))
		{
			exclude = cfg_listelt_value(element);
			CHECK(dns_name_fromstring(name,
						  cfg_obj_asstring(exclude),
						  0, NULL));
			CHECK(dns_view_excludedelegationonly(view, name));
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
		empty_zones_enable = view->recursion;
	} else if (view->rdclass == dns_rdataclass_in) {
		if (obj != NULL)
			empty_zones_enable = cfg_obj_asboolean(obj);
		else
			empty_zones_enable = view->recursion;
	} else {
		empty_zones_enable = ISC_FALSE;
	}
	if (empty_zones_enable && !lwresd_g_useresolvconf) {
		const char *empty;
		int empty_zone = 0;
		dns_fixedname_t fixed;
		dns_name_t *name;
		isc_buffer_t buffer;
		char server[DNS_NAME_FORMATSIZE + 1];
		char contact[DNS_NAME_FORMATSIZE + 1];
		const char *empty_dbtype[4] =
				    { "_builtin", "empty", NULL, NULL };
		int empty_dbtypec = 4;
		dns_zonestat_level_t statlevel;

		dns_fixedname_init(&fixed);
		name = dns_fixedname_name(&fixed);

		obj = NULL;
		result = ns_config_get(maps, "empty-server", &obj);
		if (result == ISC_R_SUCCESS) {
			CHECK(dns_name_fromstring(name, cfg_obj_asstring(obj),
						  0, NULL));
			isc_buffer_init(&buffer, server, sizeof(server) - 1);
			CHECK(dns_name_totext(name, ISC_FALSE, &buffer));
			server[isc_buffer_usedlength(&buffer)] = 0;
			empty_dbtype[2] = server;
		} else
			empty_dbtype[2] = "@";

		obj = NULL;
		result = ns_config_get(maps, "empty-contact", &obj);
		if (result == ISC_R_SUCCESS) {
			CHECK(dns_name_fromstring(name, cfg_obj_asstring(obj),
						 0, NULL));
			isc_buffer_init(&buffer, contact, sizeof(contact) - 1);
			CHECK(dns_name_totext(name, ISC_FALSE, &buffer));
			contact[isc_buffer_usedlength(&buffer)] = 0;
			empty_dbtype[3] = contact;
		} else
			empty_dbtype[3] = ".";

		obj = NULL;
		result = ns_config_get(maps, "zone-statistics", &obj);
		INSIST(result == ISC_R_SUCCESS);
		if (cfg_obj_isboolean(obj)) {
			if (cfg_obj_asboolean(obj))
				statlevel = dns_zonestat_full;
			else
				statlevel = dns_zonestat_terse; /* XXX */
		} else {
			const char *levelstr = cfg_obj_asstring(obj);
			if (strcasecmp(levelstr, "full") == 0)
				statlevel = dns_zonestat_full;
			else if (strcasecmp(levelstr, "terse") == 0)
				statlevel = dns_zonestat_terse;
			else if (strcasecmp(levelstr, "none") == 0)
				statlevel = dns_zonestat_none;
			else
				INSIST(0);
		}

		for (empty = empty_zones[empty_zone];
		     empty != NULL;
		     empty = empty_zones[++empty_zone])
		{
			dns_forwarders_t *dnsforwarders = NULL;

			/*
			 * Look for zone on drop list.
			 */
			CHECK(dns_name_fromstring(name, empty, 0, NULL));
			if (disablelist != NULL &&
			    on_disable_list(disablelist, name))
				continue;

			/*
			 * This zone already exists.
			 */
			(void)dns_view_findzone(view, name, &zone);
			if (zone != NULL) {
				dns_zone_detach(&zone);
				continue;
			}

			/*
			 * If we would forward this name don't add a
			 * empty zone for it.
			 */
			result = dns_fwdtable_find(view->fwdtable, name,
						   &dnsforwarders);
			if (result == ISC_R_SUCCESS &&
			    dnsforwarders->fwdpolicy == dns_fwdpolicy_only)
				continue;

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
			}

			CHECK(create_empty_zone(zone, name, view, zonelist,
						empty_dbtype, empty_dbtypec,
						statlevel));
			if (zone != NULL)
				dns_zone_detach(&zone);
		}
	}

#ifdef USE_RRL
	obj = NULL;
	result = ns_config_get(maps, "rate-limit", &obj);
	if (result == ISC_R_SUCCESS) {
		result = configure_rrl(view, config, obj);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
	}
#endif /* USE_RRL */

	result = ISC_R_SUCCESS;

 cleanup:
	if (clients != NULL)
		dns_acl_detach(&clients);
	if (mapped != NULL)
		dns_acl_detach(&mapped);
	if (excluded != NULL)
		dns_acl_detach(&excluded);
	if (ring != NULL)
		dns_tsigkeyring_detach(&ring);
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
	if (hmctx != NULL)
		isc_mem_detach(&hmctx);

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

			isc_buffer_constinit(&buffer, str, strlen(str));
			isc_buffer_add(&buffer, strlen(str));
			dns_fixedname_init(&fixed);
			name = dns_fixedname_name(&fixed);
			CHECK(dns_name_fromtext(name, &buffer, dns_rootname, 0,
						NULL));

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

static isc_result_t
get_viewinfo(const cfg_obj_t *vconfig, const char **namep,
	     dns_rdataclass_t *classp)
{
	isc_result_t result = ISC_R_SUCCESS;
	const char *viewname;
	dns_rdataclass_t viewclass;

	REQUIRE(namep != NULL && *namep == NULL);
	REQUIRE(classp != NULL);

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

	*namep = viewname;
	*classp = viewclass;

	return (result);
}

/*
 * Find a view based on its configuration info and attach to it.
 *
 * If 'vconfig' is NULL, attach to the default view.
 */
static isc_result_t
find_view(const cfg_obj_t *vconfig, dns_viewlist_t *viewlist,
	  dns_view_t **viewp)
{
	isc_result_t result;
	const char *viewname = NULL;
	dns_rdataclass_t viewclass;
	dns_view_t *view = NULL;

	result = get_viewinfo(vconfig, &viewname, &viewclass);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = dns_viewlist_find(viewlist, viewname, viewclass, &view);
	if (result != ISC_R_SUCCESS)
		return (result);

	*viewp = view;
	return (ISC_R_SUCCESS);
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
	const char *viewname = NULL;
	dns_rdataclass_t viewclass;
	dns_view_t *view = NULL;

	result = get_viewinfo(vconfig, &viewname, &viewclass);
	if (result != ISC_R_SUCCESS)
		return (result);

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
	       cfg_aclconfctx_t *aclconf, isc_boolean_t added)
{
	dns_view_t *pview = NULL;	/* Production view */
	dns_zone_t *zone = NULL;	/* New or reused zone */
	dns_zone_t *raw = NULL;		/* New or reused raw zone */
	dns_zone_t *dupzone = NULL;
	const cfg_obj_t *options = NULL;
	const cfg_obj_t *zoptions = NULL;
	const cfg_obj_t *typeobj = NULL;
	const cfg_obj_t *forwarders = NULL;
	const cfg_obj_t *forwardtype = NULL;
	const cfg_obj_t *only = NULL;
	const cfg_obj_t *signing = NULL;
	isc_result_t result;
	isc_result_t tresult;
	isc_buffer_t buffer;
	dns_fixedname_t fixorigin;
	dns_name_t *origin;
	const char *zname;
	dns_rdataclass_t zclass;
	const char *ztypestr;
	isc_boolean_t is_rpz;
	dns_rpz_zone_t *rpz;

	options = NULL;
	(void)cfg_map_get(config, "options", &options);

	zoptions = cfg_tuple_get(zconfig, "options");

	/*
	 * Get the zone origin as a dns_name_t.
	 */
	zname = cfg_obj_asstring(cfg_tuple_get(zconfig, "name"));
	isc_buffer_constinit(&buffer, zname, strlen(zname));
	isc_buffer_add(&buffer, strlen(zname));
	dns_fixedname_init(&fixorigin);
	CHECK(dns_name_fromtext(dns_fixedname_name(&fixorigin),
				&buffer, dns_rootname, 0, NULL));
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
	 * "hints zones" aren't zones.	If we've got one,
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

			CHECK(configure_hints(view, hintsfile));

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
		CHECK(configure_forward(config, view, origin, forwarders,
					forwardtype));

		/*
		 * Forward zones may also set delegation only.
		 */
		only = NULL;
		tresult = cfg_map_get(zoptions, "delegation-only", &only);
		if (tresult == ISC_R_SUCCESS && cfg_obj_asboolean(only))
			CHECK(dns_view_adddelegationonly(view, origin));
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
	 * Redirect zones only require minimal configuration.
	 */
	if (strcasecmp(ztypestr, "redirect") == 0) {
		if (view->redirect != NULL) {
			cfg_obj_log(zconfig, ns_g_lctx, ISC_LOG_ERROR,
				    "redirect zone already exists");
			result = ISC_R_EXISTS;
			goto cleanup;
		}
		result = dns_viewlist_find(&ns_g_server->viewlist, view->name,
					   view->rdclass, &pview);
		if (result != ISC_R_NOTFOUND && result != ISC_R_SUCCESS)
			goto cleanup;
		if (pview != NULL && pview->redirect != NULL) {
			dns_zone_attach(pview->redirect, &zone);
			dns_zone_setview(zone, view);
		} else {
			CHECK(dns_zonemgr_createzone(ns_g_server->zonemgr,
						     &zone));
			CHECK(dns_zone_setorigin(zone, origin));
			dns_zone_setview(zone, view);
			CHECK(dns_zonemgr_managezone(ns_g_server->zonemgr,
						     zone));
			dns_zone_setstats(zone, ns_g_server->zonestats);
		}
		CHECK(ns_zone_configure(config, vconfig, zconfig, aclconf,
					zone, NULL));
		dns_zone_attach(zone, &view->redirect);
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
	 * Note whether this is a response policy zone.
	 */
	is_rpz = ISC_FALSE;
	for (rpz = ISC_LIST_HEAD(view->rpz_zones);
	     rpz != NULL;
	     rpz = ISC_LIST_NEXT(rpz, link))
	{
		if (dns_name_equal(&rpz->origin, origin)) {
			is_rpz = ISC_TRUE;
			rpz->defined = ISC_TRUE;
			break;
		}
	}

	/*
	 * See if we can reuse an existing zone.  This is
	 * only possible if all of these are true:
	 *   - The zone's view exists
	 *   - A zone with the right name exists in the view
	 *   - The zone is compatible with the config
	 *     options (e.g., an existing master zone cannot
	 *     be reused if the options specify a slave zone)
	 *   - The zone was and is or was not and is not a policy zone
	 */
	result = dns_viewlist_find(&ns_g_server->viewlist, view->name,
				   view->rdclass, &pview);
	if (result != ISC_R_NOTFOUND && result != ISC_R_SUCCESS)
		goto cleanup;
	if (pview != NULL)
		result = dns_view_findzone(pview, origin, &zone);
	if (result != ISC_R_NOTFOUND && result != ISC_R_SUCCESS)
		goto cleanup;

	if (zone != NULL && !ns_zone_reusable(zone, zconfig))
		dns_zone_detach(&zone);

	if (zone != NULL && is_rpz != dns_zone_get_rpz(zone))
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
		CHECK(dns_zonemgr_createzone(ns_g_server->zonemgr, &zone));
		CHECK(dns_zone_setorigin(zone, origin));
		dns_zone_setview(zone, view);
		if (view->acache != NULL)
			dns_zone_setacache(zone, view->acache);
		CHECK(dns_zonemgr_managezone(ns_g_server->zonemgr, zone));
		dns_zone_setstats(zone, ns_g_server->zonestats);
	}

	if (is_rpz) {
		result = dns_zone_rpz_enable(zone);
		if (result != ISC_R_SUCCESS) {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
				      "zone '%s': incompatible"
				      " masterfile-format or database"
				      " for a response policy zone",
				      zname);
			goto cleanup;
		}
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
	 * Mark whether the zone was originally added at runtime or not
	 */
	dns_zone_setadded(zone, added);

	signing = NULL;
	if ((strcasecmp(ztypestr, "master") == 0 ||
	     strcasecmp(ztypestr, "slave") == 0) &&
	    cfg_map_get(zoptions, "inline-signing", &signing) == ISC_R_SUCCESS &&
	    cfg_obj_asboolean(signing))
	{
		dns_zone_getraw(zone, &raw);
		if (raw == NULL) {
			CHECK(dns_zone_create(&raw, mctx));
			CHECK(dns_zone_setorigin(raw, origin));
			dns_zone_setview(raw, view);
			if (view->acache != NULL)
				dns_zone_setacache(raw, view->acache);
			dns_zone_setstats(raw, ns_g_server->zonestats);
			CHECK(dns_zone_link(zone, raw));
		}
	}

	/*
	 * Configure the zone.
	 */
	CHECK(ns_zone_configure(config, vconfig, zconfig, aclconf, zone, raw));

	/*
	 * Add the zone to its view in the new view list.
	 */
	CHECK(dns_view_addzone(view, zone));

	/*
	 * Ensure that zone keys are reloaded on reconfig
	 */
	if ((dns_zone_getkeyopts(zone) & DNS_ZONEKEY_MAINTAIN) != 0)
		dns_zone_rekey(zone, ISC_FALSE);

 cleanup:
	if (zone != NULL)
		dns_zone_detach(&zone);
	if (raw != NULL)
		dns_zone_detach(&raw);
	if (pview != NULL)
		dns_view_detach(&pview);

	return (result);
}

/*
 * Configure built-in zone for storing managed-key data.
 */

#define KEYZONE "managed-keys.bind"
#define MKEYS ".mkeys"

static isc_result_t
add_keydata_zone(dns_view_t *view, const char *directory, isc_mem_t *mctx) {
	isc_result_t result;
	dns_view_t *pview = NULL;
	dns_zone_t *zone = NULL;
	dns_acl_t *none = NULL;
	char filename[PATH_MAX];
	char buffer[ISC_SHA256_DIGESTSTRINGLENGTH + sizeof(MKEYS)];
	int n;

	REQUIRE(view != NULL);

	/* See if we can re-use an existing keydata zone. */
	result = dns_viewlist_find(&ns_g_server->viewlist,
				   view->name, view->rdclass,
				   &pview);
	if (result != ISC_R_NOTFOUND &&
	    result != ISC_R_SUCCESS)
		return (result);

	if (pview != NULL && pview->managed_keys != NULL) {
		dns_zone_attach(pview->managed_keys, &view->managed_keys);
		dns_zone_setview(pview->managed_keys, view);
		dns_view_detach(&pview);
		dns_zone_synckeyzone(view->managed_keys);
		return (ISC_R_SUCCESS);
	}

	/* No existing keydata zone was found; create one */
	CHECK(dns_zonemgr_createzone(ns_g_server->zonemgr, &zone));
	CHECK(dns_zone_setorigin(zone, dns_rootname));

	isc_sha256_data((void *)view->name, strlen(view->name), buffer);
	strcat(buffer, MKEYS);
	n = snprintf(filename, sizeof(filename), "%s%s%s",
		     directory ? directory : "", directory ? "/" : "",
		     strcmp(view->name, "_default") == 0 ? KEYZONE : buffer);
	if (n < 0 || (size_t)n >= sizeof(filename)) {
		result = (n < 0) ? ISC_R_FAILURE : ISC_R_NOSPACE;
		goto cleanup;
	}
	CHECK(dns_zone_setfile(zone, filename));

	dns_zone_setview(zone, view);
	dns_zone_settype(zone, dns_zone_key);
	dns_zone_setclass(zone, view->rdclass);

	CHECK(dns_zonemgr_managezone(ns_g_server->zonemgr, zone));

	if (view->acache != NULL)
		dns_zone_setacache(zone, view->acache);

	CHECK(dns_acl_none(mctx, &none));
	dns_zone_setqueryacl(zone, none);
	dns_zone_setqueryonacl(zone, none);
	dns_acl_detach(&none);

	dns_zone_setdialup(zone, dns_dialuptype_no);
	dns_zone_setnotifytype(zone, dns_notifytype_no);
	dns_zone_setoption(zone, DNS_ZONEOPT_NOCHECKNS, ISC_TRUE);
	dns_zone_setjournalsize(zone, 0);

	dns_zone_setstats(zone, ns_g_server->zonestats);
	CHECK(setquerystats(zone, mctx, dns_zonestat_none));

	if (view->managed_keys != NULL)
		dns_zone_detach(&view->managed_keys);
	dns_zone_attach(zone, &view->managed_keys);

	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
		      "set up managed keys zone for view %s, file '%s'",
		      view->name, filename);

cleanup:
	if (zone != NULL)
		dns_zone_detach(&zone);
	if (none != NULL)
		dns_acl_detach(&none);

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
	case dns_zone_staticstub:
		type = "static-stub";
		break;
	case dns_zone_redirect:
		type = "redirect";
		break;
	default:
		type = "other";
		break;
	}
	dns_zone_log(zone, ISC_LOG_INFO, "(%s) removed", type);
	return (ISC_R_SUCCESS);
}

static void
cleanup_session_key(ns_server_t *server, isc_mem_t *mctx) {
	if (server->session_keyfile != NULL) {
		isc_file_remove(server->session_keyfile);
		isc_mem_free(mctx, server->session_keyfile);
		server->session_keyfile = NULL;
	}

	if (server->session_keyname != NULL) {
		if (dns_name_dynamic(server->session_keyname))
			dns_name_free(server->session_keyname, mctx);
		isc_mem_put(mctx, server->session_keyname, sizeof(dns_name_t));
		server->session_keyname = NULL;
	}

	if (server->sessionkey != NULL)
		dns_tsigkey_detach(&server->sessionkey);

	server->session_keyalg = DST_ALG_UNKNOWN;
	server->session_keybits = 0;
}

static isc_result_t
generate_session_key(const char *filename, const char *keynamestr,
		     dns_name_t *keyname, const char *algstr,
		     dns_name_t *algname, unsigned int algtype,
		     isc_uint16_t bits, isc_mem_t *mctx,
		     dns_tsigkey_t **tsigkeyp)
{
	isc_result_t result = ISC_R_SUCCESS;
	dst_key_t *key = NULL;
	isc_buffer_t key_txtbuffer;
	isc_buffer_t key_rawbuffer;
	char key_txtsecret[256];
	char key_rawsecret[64];
	isc_region_t key_rawregion;
	isc_stdtime_t now;
	dns_tsigkey_t *tsigkey = NULL;
	FILE *fp = NULL;

	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
		      "generating session key for dynamic DNS");

	/* generate key */
	result = dst_key_generate(keyname, algtype, bits, 1, 0,
				  DNS_KEYPROTO_ANY, dns_rdataclass_in,
				  mctx, &key);
	if (result != ISC_R_SUCCESS)
		return (result);

	/*
	 * Dump the key to the buffer for later use.  Should be done before
	 * we transfer the ownership of key to tsigkey.
	 */
	isc_buffer_init(&key_rawbuffer, &key_rawsecret, sizeof(key_rawsecret));
	CHECK(dst_key_tobuffer(key, &key_rawbuffer));

	isc_buffer_usedregion(&key_rawbuffer, &key_rawregion);
	isc_buffer_init(&key_txtbuffer, &key_txtsecret, sizeof(key_txtsecret));
	CHECK(isc_base64_totext(&key_rawregion, -1, "", &key_txtbuffer));

	/* Store the key in tsigkey. */
	isc_stdtime_get(&now);
	CHECK(dns_tsigkey_createfromkey(dst_key_name(key), algname, key,
					ISC_FALSE, NULL, now, now, mctx, NULL,
					&tsigkey));

	/* Dump the key to the key file. */
	fp = ns_os_openfile(filename, S_IRUSR|S_IWUSR, ISC_TRUE);
	if (fp == NULL) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "could not create %s", filename);
		result = ISC_R_NOPERM;
		goto cleanup;
	}

	fprintf(fp, "key \"%s\" {\n"
		"\talgorithm %s;\n"
		"\tsecret \"%.*s\";\n};\n", keynamestr, algstr,
		(int) isc_buffer_usedlength(&key_txtbuffer),
		(char*) isc_buffer_base(&key_txtbuffer));

	CHECK(isc_stdio_flush(fp));
	CHECK(isc_stdio_close(fp));

	dst_key_free(&key);

	*tsigkeyp = tsigkey;

	return (ISC_R_SUCCESS);

  cleanup:
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
		      "failed to generate session key "
		      "for dynamic DNS: %s", isc_result_totext(result));
	if (fp != NULL) {
		if (isc_file_exists(filename))
			(void)isc_file_remove(filename);
		(void)isc_stdio_close(fp);
	}
	if (tsigkey != NULL)
		dns_tsigkey_detach(&tsigkey);
	if (key != NULL)
		dst_key_free(&key);

	return (result);
}

static isc_result_t
configure_session_key(const cfg_obj_t **maps, ns_server_t *server,
		      isc_mem_t *mctx)
{
	const char *keyfile, *keynamestr, *algstr;
	unsigned int algtype;
	dns_fixedname_t fname;
	dns_name_t *keyname, *algname;
	isc_buffer_t buffer;
	isc_uint16_t bits;
	const cfg_obj_t *obj;
	isc_boolean_t need_deleteold = ISC_FALSE;
	isc_boolean_t need_createnew = ISC_FALSE;
	isc_result_t result;

	obj = NULL;
	result = ns_config_get(maps, "session-keyfile", &obj);
	if (result == ISC_R_SUCCESS) {
		if (cfg_obj_isvoid(obj))
			keyfile = NULL; /* disable it */
		else
			keyfile = cfg_obj_asstring(obj);
	} else
		keyfile = ns_g_defaultsessionkeyfile;

	obj = NULL;
	result = ns_config_get(maps, "session-keyname", &obj);
	INSIST(result == ISC_R_SUCCESS);
	keynamestr = cfg_obj_asstring(obj);
	dns_fixedname_init(&fname);
	isc_buffer_constinit(&buffer, keynamestr, strlen(keynamestr));
	isc_buffer_add(&buffer, strlen(keynamestr));
	keyname = dns_fixedname_name(&fname);
	result = dns_name_fromtext(keyname, &buffer, dns_rootname, 0, NULL);
	if (result != ISC_R_SUCCESS)
		return (result);

	obj = NULL;
	result = ns_config_get(maps, "session-keyalg", &obj);
	INSIST(result == ISC_R_SUCCESS);
	algstr = cfg_obj_asstring(obj);
	algname = NULL;
	result = ns_config_getkeyalgorithm2(algstr, &algname, &algtype, &bits);
	if (result != ISC_R_SUCCESS) {
		const char *s = " (keeping current key)";

		cfg_obj_log(obj, ns_g_lctx, ISC_LOG_ERROR, "session-keyalg: "
			    "unsupported or unknown algorithm '%s'%s",
			    algstr,
			    server->session_keyfile != NULL ? s : "");
		return (result);
	}

	/* See if we need to (re)generate a new key. */
	if (keyfile == NULL) {
		if (server->session_keyfile != NULL)
			need_deleteold = ISC_TRUE;
	} else if (server->session_keyfile == NULL)
		need_createnew = ISC_TRUE;
	else if (strcmp(keyfile, server->session_keyfile) != 0 ||
		 !dns_name_equal(server->session_keyname, keyname) ||
		 server->session_keyalg != algtype ||
		 server->session_keybits != bits) {
		need_deleteold = ISC_TRUE;
		need_createnew = ISC_TRUE;
	}

	if (need_deleteold) {
		INSIST(server->session_keyfile != NULL);
		INSIST(server->session_keyname != NULL);
		INSIST(server->sessionkey != NULL);

		cleanup_session_key(server, mctx);
	}

	if (need_createnew) {
		INSIST(server->sessionkey == NULL);
		INSIST(server->session_keyfile == NULL);
		INSIST(server->session_keyname == NULL);
		INSIST(server->session_keyalg == DST_ALG_UNKNOWN);
		INSIST(server->session_keybits == 0);

		server->session_keyname = isc_mem_get(mctx, sizeof(dns_name_t));
		if (server->session_keyname == NULL)
			goto cleanup;
		dns_name_init(server->session_keyname, NULL);
		CHECK(dns_name_dup(keyname, mctx, server->session_keyname));

		server->session_keyfile = isc_mem_strdup(mctx, keyfile);
		if (server->session_keyfile == NULL)
			goto cleanup;

		server->session_keyalg = algtype;
		server->session_keybits = bits;

		CHECK(generate_session_key(keyfile, keynamestr, keyname, algstr,
					   algname, algtype, bits, mctx,
					   &server->sessionkey));
	}

	return (result);

  cleanup:
	cleanup_session_key(server, mctx);
	return (result);
}

static isc_result_t
setup_newzones(dns_view_t *view, cfg_obj_t *config, cfg_obj_t *vconfig,
	       cfg_parser_t *parser, cfg_aclconfctx_t *actx)
{
	isc_result_t result = ISC_R_SUCCESS;
	isc_boolean_t allow = ISC_FALSE;
	struct cfg_context *nzcfg = NULL;
	cfg_parser_t *nzparser = NULL;
	cfg_obj_t *nzconfig = NULL;
	const cfg_obj_t *maps[4];
	const cfg_obj_t *options = NULL, *voptions = NULL;
	const cfg_obj_t *nz = NULL;
	int i = 0;

	REQUIRE (config != NULL);

	if (vconfig != NULL)
		voptions = cfg_tuple_get(vconfig, "options");
	if (voptions != NULL)
		maps[i++] = voptions;
	result = cfg_map_get(config, "options", &options);
	if (result == ISC_R_SUCCESS)
		maps[i++] = options;
	maps[i++] = ns_g_defaults;
	maps[i] = NULL;

	result = ns_config_get(maps, "allow-new-zones", &nz);
	if (result == ISC_R_SUCCESS)
		allow = cfg_obj_asboolean(nz);

	if (!allow) {
		dns_view_setnewzones(view, ISC_FALSE, NULL, NULL);
		return (ISC_R_SUCCESS);
	}

	nzcfg = isc_mem_get(view->mctx, sizeof(*nzcfg));
	if (nzcfg == NULL) {
		dns_view_setnewzones(view, ISC_FALSE, NULL, NULL);
		return (ISC_R_NOMEMORY);
	}

	dns_view_setnewzones(view, allow, nzcfg, newzone_cfgctx_destroy);

	memset(nzcfg, 0, sizeof(*nzcfg));
	isc_mem_attach(view->mctx, &nzcfg->mctx);
	cfg_obj_attach(config, &nzcfg->config);
	cfg_parser_attach(parser, &nzcfg->parser);
	cfg_aclconfctx_attach(actx, &nzcfg->actx);

	/*
	 * Attempt to create a parser and parse the newzones
	 * file.  If successful, preserve both; otherwise leave
	 * them NULL.
	 */
	result = cfg_parser_create(view->mctx, ns_g_lctx, &nzparser);
	if (result == ISC_R_SUCCESS)
		result = cfg_parse_file(nzparser, view->new_zone_file,
					&cfg_type_newzones, &nzconfig);
	if (result == ISC_R_SUCCESS) {
		cfg_parser_attach(nzparser, &nzcfg->nzparser);
		cfg_obj_attach(nzconfig, &nzcfg->nzconfig);
	}

	if (nzparser != NULL) {
		if (nzconfig != NULL)
			cfg_obj_destroy(nzparser, &nzconfig);
		cfg_parser_destroy(&nzparser);
	}

	return (ISC_R_SUCCESS);
}

static int
count_zones(const cfg_obj_t *conf) {
	const cfg_obj_t *zonelist = NULL;
	const cfg_listelt_t *element;
	int n = 0;

	REQUIRE(conf != NULL);

	cfg_map_get(conf, "zone", &zonelist);
	for (element = cfg_list_first(zonelist);
	     element != NULL;
	     element = cfg_list_next(element))
		n++;

	return (n);
}

static isc_result_t
load_configuration(const char *filename, ns_server_t *server,
		   isc_boolean_t first_time)
{
	cfg_obj_t *config = NULL, *bindkeys = NULL;
	cfg_parser_t *conf_parser = NULL, *bindkeys_parser = NULL;
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
	dns_viewlist_t viewlist, builtin_viewlist;
	in_port_t listen_port, udpport_low, udpport_high;
	int i;
	int num_zones = 0;
	isc_boolean_t exclusive = ISC_FALSE;
	isc_interval_t interval;
	isc_logconfig_t *logc = NULL;
	isc_portset_t *v4portset = NULL;
	isc_portset_t *v6portset = NULL;
	isc_resourcevalue_t nfiles;
	isc_result_t result;
	isc_uint32_t heartbeat_interval;
	isc_uint32_t interface_interval;
	isc_uint32_t reserved;
	isc_uint32_t udpsize;
	ns_cache_t *nsc;
	ns_cachelist_t cachelist, tmpcachelist;
	struct cfg_context *nzctx;
	unsigned int maxsocks;
#ifdef ENABLE_FETCHLIMIT
	isc_uint32_t softquota = 0;
#endif /* ENABLE_FETCHLIMIT */

	ISC_LIST_INIT(viewlist);
	ISC_LIST_INIT(builtin_viewlist);
	ISC_LIST_INIT(cachelist);

	/* Create the ACL configuration context */
	if (ns_g_aclconfctx != NULL)
		cfg_aclconfctx_detach(&ns_g_aclconfctx);
	CHECK(cfg_aclconfctx_create(ns_g_mctx, &ns_g_aclconfctx));

	/*
	 * Parse the global default pseudo-config file.
	 */
	if (first_time) {
		CHECK(ns_config_parsedefaults(ns_g_parser, &ns_g_config));
		RUNTIME_CHECK(cfg_map_get(ns_g_config, "options",
					  &ns_g_defaults) == ISC_R_SUCCESS);
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
		CHECK(cfg_parser_create(ns_g_mctx, ns_g_lctx, &conf_parser));
		cfg_parser_setcallback(conf_parser, directory_callback, NULL);
		result = cfg_parse_file(conf_parser, filename,
					&cfg_type_namedconf, &config);
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
		if (conf_parser != NULL)
			cfg_parser_destroy(&conf_parser);
		CHECK(cfg_parser_create(ns_g_mctx, ns_g_lctx, &conf_parser));
		result = ns_lwresd_parseeresolvconf(ns_g_mctx, conf_parser,
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
	maps[i] = NULL;

	/*
	 * If bind.keys exists, load it.  If "dnssec-lookaside auto"
	 * is turned on, the keys found there will be used as default
	 * trust anchors.
	 */
	obj = NULL;
	result = ns_config_get(maps, "bindkeys-file", &obj);
	INSIST(result == ISC_R_SUCCESS);
	CHECKM(setstring(server, &server->bindkeysfile,
	       cfg_obj_asstring(obj)), "strdup");

	if (access(server->bindkeysfile, R_OK) == 0) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
			      "reading built-in trusted "
			      "keys from file '%s'", server->bindkeysfile);

		CHECK(cfg_parser_create(ns_g_mctx, ns_g_lctx,
					&bindkeys_parser));

		result = cfg_parse_file(bindkeys_parser, server->bindkeysfile,
					&cfg_type_bindkeys, &bindkeys);
		CHECK(result);
	}

	/* Ensure exclusive access to configuration data. */
	if (!exclusive) {
		result = isc_task_beginexclusive(server->task);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		exclusive = ISC_TRUE;
	}

	/*
	 * Set process limits, which (usually) needs to be done as root.
	 */
	set_limits(maps);

	/*
	 * Check if max number of open sockets that the system allows is
	 * sufficiently large.	Failing this condition is not necessarily fatal,
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

#ifdef ENABLE_FETCHLIMIT
	if (server->recursionquota.max > 1000) {
		int margin = ISC_MAX(100, ns_g_cpus + 1);
		if (margin > server->recursionquota.max - 100) {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
				      "'recursive-clients %d' too low when "
				      "running with %d worker threads",
				      server->recursionquota.max, ns_g_cpus);
			CHECK(ISC_R_RANGE);
		}
		softquota = server->recursionquota.max - margin;
	} else
		softquota = (server->recursionquota.max * 90) / 100;

	isc_quota_soft(&server->recursionquota, softquota);
#else
	if (server->recursionquota.max > 1000) {
		isc_quota_soft(&server->recursionquota,
			       server->recursionquota.max - 100);
	} else
		isc_quota_soft(&server->recursionquota, 0);
#endif /* !ENABLE_FETCHLIMIT */

	CHECK(configure_view_acl(NULL, config, "blackhole", NULL,
				 ns_g_aclconfctx, ns_g_mctx,
				 &server->blackholeacl));
	if (server->blackholeacl != NULL)
		dns_dispatchmgr_setblackhole(ns_g_dispatchmgr,
					     server->blackholeacl);

	obj = NULL;
	result = ns_config_get(maps, "match-mapped-addresses", &obj);
	INSIST(result == ISC_R_SUCCESS);
	server->aclenv.match_mapped = cfg_obj_asboolean(obj);

	CHECKM(ns_statschannels_configure(ns_g_server, config, ns_g_aclconfctx),
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
		if (!ns_g_disable4)
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
				      "using default UDP/IPv4 port range: "
				      "[%d, %d]", udpport_low, udpport_high);
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
		if (!ns_g_disable6)
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
				      "using default UDP/IPv6 port range: "
				      "[%d, %d]", udpport_low, udpport_high);
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
	if ((ns_g_listen > 0) && (ns_g_listen < 10))
		ns_g_listen = 10;

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
			/* check return code? */
			(void)ns_listenlist_fromconfig(clistenon, config,
						       ns_g_aclconfctx,
						       ns_g_mctx, AF_INET,
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
			/* check return code? */
			(void)ns_listenlist_fromconfig(clistenon, config,
						       ns_g_aclconfctx,
						       ns_g_mctx, AF_INET6,
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
	 * Write the PID file.
	 */
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

	/*
	 * Configure the server-wide session key.  This must be done before
	 * configure views because zone configuration may need to know
	 * session-keyname.
	 *
	 * Failure of session key generation isn't fatal at this time; if it
	 * turns out that a session key is really needed but doesn't exist,
	 * we'll treat it as a fatal error then.
	 */
	(void)configure_session_key(maps, server, ns_g_mctx);

	views = NULL;
	(void)cfg_map_get(config, "view", &views);

	/*
	 * Create the views and count all the configured zones in
	 * order to correctly size the zone manager's task table.
	 * (We only count zones for configured views; the built-in
	 * "bind" view can be ignored as it only adds a negligible
	 * number of zones.)
	 *
	 * If we're allowing new zones, we need to be able to find the
	 * new zone file and count those as well.  So we setup the new
	 * zone configuration context, but otherwise view configuration
	 * waits until after the zone manager's task list has been sized.
	 */
	for (element = cfg_list_first(views);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		cfg_obj_t *vconfig = cfg_listelt_value(element);
		const cfg_obj_t *voptions = cfg_tuple_get(vconfig, "options");
		view = NULL;

		CHECK(create_view(vconfig, &viewlist, &view));
		INSIST(view != NULL);

		num_zones += count_zones(voptions);
		CHECK(setup_newzones(view, config, vconfig, conf_parser,
				     ns_g_aclconfctx));

		nzctx = view->new_zone_config;
		if (nzctx != NULL && nzctx->nzconfig != NULL)
			num_zones += count_zones(nzctx->nzconfig);

		dns_view_detach(&view);
	}

	/*
	 * If there were no explicit views then we do the default
	 * view here.
	 */
	if (views == NULL) {
		CHECK(create_view(NULL, &viewlist, &view));
		INSIST(view != NULL);

		num_zones = count_zones(config);

		CHECK(setup_newzones(view, config, NULL,  conf_parser,
				     ns_g_aclconfctx));

		nzctx = view->new_zone_config;
		if (nzctx != NULL && nzctx->nzconfig != NULL)
			num_zones += count_zones(nzctx->nzconfig);

		dns_view_detach(&view);
	}

	/*
	 * Zones have been counted; set the zone manager task pool size.
	 */
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
		      "sizing zone task pool based on %d zones", num_zones);
	CHECK(dns_zonemgr_setsize(ns_g_server->zonemgr, num_zones));

	/*
	 * Configure and freeze all explicit views.  Explicit
	 * views that have zones were already created at parsing
	 * time, but views with no zones must be created here.
	 */
	for (element = cfg_list_first(views);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		cfg_obj_t *vconfig = cfg_listelt_value(element);

		view = NULL;
		CHECK(find_view(vconfig, &viewlist, &view));
		CHECK(configure_view(view, config, vconfig,
				     &cachelist, bindkeys, ns_g_mctx,
				     ns_g_aclconfctx, ISC_TRUE));
		dns_view_freeze(view);
		dns_view_detach(&view);
	}

	/*
	 * Make sure we have a default view if and only if there
	 * were no explicit views.
	 */
	if (views == NULL) {
		view = NULL;
		CHECK(find_view(NULL, &viewlist, &view));
		CHECK(configure_view(view, config, NULL,
				     &cachelist, bindkeys,
				     ns_g_mctx, ns_g_aclconfctx, ISC_TRUE));
		dns_view_freeze(view);
		dns_view_detach(&view);
	}

	/*
	 * Create (or recreate) the built-in views.
	 */
	builtin_views = NULL;
	RUNTIME_CHECK(cfg_map_get(ns_g_config, "view",
				  &builtin_views) == ISC_R_SUCCESS);
	for (element = cfg_list_first(builtin_views);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		cfg_obj_t *vconfig = cfg_listelt_value(element);

		CHECK(create_view(vconfig, &builtin_viewlist, &view));
		CHECK(configure_view(view, config, vconfig,
				     &cachelist, bindkeys,
				     ns_g_mctx, ns_g_aclconfctx, ISC_FALSE));
		dns_view_freeze(view);
		dns_view_detach(&view);
		view = NULL;
	}

	/* Now combine the two viewlists into one */
	ISC_LIST_APPENDLIST(viewlist, builtin_viewlist, link);

	/* Swap our new view list with the production one. */
	tmpviewlist = server->viewlist;
	server->viewlist = viewlist;
	viewlist = tmpviewlist;

	/* Make the view list available to each of the views */
	view = ISC_LIST_HEAD(server->viewlist);
	while (view != NULL) {
		view->viewlist = &server->viewlist;
		view = ISC_LIST_NEXT(view, link);
	}

	/* Swap our new cache list with the production one. */
	tmpcachelist = server->cachelist;
	server->cachelist = cachelist;
	cachelist = tmpcachelist;

	/* Load the TKEY information from the configuration. */
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
				     ns_g_aclconfctx),
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
		const cfg_obj_t *logobj = NULL;

		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
			      "not using config file logging "
			      "statement for logging due to "
			      "-g option");

		(void)cfg_map_get(config, "logging", &logobj);
		if (logobj != NULL) {
			result = ns_log_configure(NULL, logobj);
			if (result != ISC_R_SUCCESS) {
				isc_log_write(ns_g_lctx,
					      NS_LOGCATEGORY_GENERAL,
					      NS_LOGMODULE_SERVER,
					      ISC_LOG_ERROR,
					      "checking logging configuration "
					      "failed: %s",
					      isc_result_totext(result));
				goto cleanup;
			}
		}
	} else {
		const cfg_obj_t *logobj = NULL;

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

		CHECKM(isc_logconfig_use(ns_g_lctx, logc),
		       "installing logging configuration");
		logc = NULL;

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
	result = ns_config_get(maps, "secroots-file", &obj);
	INSIST(result == ISC_R_SUCCESS);
	CHECKM(setstring(server, &server->secrootsfile, cfg_obj_asstring(obj)),
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
	if (logc != NULL)
		isc_logconfig_destroy(&logc);

	if (v4portset != NULL)
		isc_portset_destroy(ns_g_mctx, &v4portset);

	if (v6portset != NULL)
		isc_portset_destroy(ns_g_mctx, &v6portset);

	if (conf_parser != NULL) {
		if (config != NULL)
			cfg_obj_destroy(conf_parser, &config);
		cfg_parser_destroy(&conf_parser);
	}

	if (bindkeys_parser != NULL) {
		if (bindkeys != NULL)
			cfg_obj_destroy(bindkeys_parser, &bindkeys);
		cfg_parser_destroy(&bindkeys_parser);
	}

	if (view != NULL)
		dns_view_detach(&view);

	ISC_LIST_APPENDLIST(viewlist, builtin_viewlist, link);

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

	/* Same cleanup for cache list. */
	while ((nsc = ISC_LIST_HEAD(cachelist)) != NULL) {
		ISC_LIST_UNLINK(cachelist, nsc, link);
		dns_cache_detach(&nsc->cache);
		isc_mem_put(server->mctx, nsc, sizeof(*nsc));
	}

	/*
	 * Adjust the listening interfaces in accordance with the source
	 * addresses specified in views and zones.
	 */
	if (isc_net_probeipv6() == ISC_R_SUCCESS)
		adjust_interfaces(server, ns_g_mctx);

	/* Relinquish exclusive access to configuration data. */
	if (exclusive)
		isc_task_endexclusive(server->task);

	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      ISC_LOG_DEBUG(1), "load_configuration: %s",
		      isc_result_totext(result));

	return (result);
}

static isc_result_t
view_loaded(void *arg) {
	isc_result_t result;
	ns_zoneload_t *zl = (ns_zoneload_t *) arg;
	ns_server_t *server = zl->server;
	unsigned int refs;


	/*
	 * Force zone maintenance.  Do this after loading
	 * so that we know when we need to force AXFR of
	 * slave zones whose master files are missing.
	 *
	 * We use the zoneload reference counter to let us
	 * know when all views are finished.
	 */
	isc_refcount_decrement(&zl->refs, &refs);
	if (refs != 0)
		return (ISC_R_SUCCESS);

	isc_refcount_destroy(&zl->refs);
	isc_mem_put(server->mctx, zl, sizeof (*zl));

	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      ISC_LOG_NOTICE, "all zones loaded");
	CHECKFATAL(dns_zonemgr_forcemaint(server->zonemgr),
		   "forcing zone maintenance");

	ns_os_started();
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      ISC_LOG_NOTICE, "running");

	return (ISC_R_SUCCESS);
}

static isc_result_t
load_zones(ns_server_t *server, isc_boolean_t init) {
	isc_result_t result;
	dns_view_t *view;
	ns_zoneload_t *zl;
	unsigned int refs = 0;

	zl = isc_mem_get(server->mctx, sizeof (*zl));
	if (zl == NULL)
		return (ISC_R_NOMEMORY);
	zl->server = server;

	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	isc_refcount_init(&zl->refs, 1);

	/*
	 * Schedule zones to be loaded from disk.
	 */
	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link))
	{
		if (view->managed_keys != NULL) {
			result = dns_zone_load(view->managed_keys);
			if (result != ISC_R_SUCCESS &&
			    result != DNS_R_UPTODATE &&
			    result != DNS_R_CONTINUE)
				goto cleanup;
		}
		if (view->redirect != NULL) {
			result = dns_zone_load(view->redirect);
			if (result != ISC_R_SUCCESS &&
			    result != DNS_R_UPTODATE &&
			    result != DNS_R_CONTINUE)
				goto cleanup;
		}

		/*
		 * 'dns_view_asyncload' calls view_loaded if there are no
		 * zones.
		 */
		isc_refcount_increment(&zl->refs, NULL);
		CHECK(dns_view_asyncload(view, view_loaded, zl));
	}

 cleanup:
	isc_refcount_decrement(&zl->refs, &refs);
	if (refs == 0) {
		isc_refcount_destroy(&zl->refs);
		isc_mem_put(server->mctx, zl, sizeof (*zl));
	} else if (init) {
		/*
		 * Place the task manager into privileged mode.  This
		 * ensures that after we leave task-exclusive mode, no
		 * other tasks will be able to run except for the ones
		 * that are loading zones. (This should only be done during
		 * the initial server setup; it isn't necessary during
		 * a reload.)
		 */
		isc_taskmgr_setmode(ns_g_taskmgr, isc_taskmgrmode_privileged);
	}

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

		/* Load managed-keys data */
		if (view->managed_keys != NULL)
			CHECK(dns_zone_loadnew(view->managed_keys));
		if (view->redirect != NULL)
			CHECK(dns_zone_loadnew(view->redirect));
	}

	/*
	 * Resume zone XFRs.
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

	CHECKFATAL(load_zones(server, ISC_TRUE), "loading zones");
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
	ns_cache_t *nsc;

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
	cleanup_session_key(server, server->mctx);

	if (ns_g_aclconfctx != NULL)
		cfg_aclconfctx_detach(&ns_g_aclconfctx);

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

	while ((nsc = ISC_LIST_HEAD(server->cachelist)) != NULL) {
		ISC_LIST_UNLINK(server->cachelist, nsc, link);
		dns_cache_detach(&nsc->cache);
		isc_mem_put(server->mctx, nsc, sizeof(*nsc));
	}

	isc_timer_detach(&server->interface_timer);
	isc_timer_detach(&server->heartbeat_timer);
	isc_timer_detach(&server->pps_timer);

	ns_interfacemgr_shutdown(server->interfacemgr);
	ns_interfacemgr_detach(&server->interfacemgr);

	dns_dispatchmgr_destroy(&ns_g_dispatchmgr);

	dns_zonemgr_shutdown(server->zonemgr);

	if (ns_g_sessionkey != NULL) {
		dns_tsigkey_detach(&ns_g_sessionkey);
		dns_name_free(&ns_g_sessionkeyname, server->mctx);
	}

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

	CHECKFATAL(dst_lib_init2(ns_g_mctx, ns_g_entropy,
				 ns_g_engine, ISC_ENTROPY_GOODONLY),
		   "initializing DST");

	server->tkeyctx = NULL;
	CHECKFATAL(dns_tkeyctx_create(ns_g_mctx, ns_g_entropy,
				      &server->tkeyctx),
		   "creating TKEY context");

	/*
	 * Setup the server task, which is responsible for coordinating
	 * startup and shutdown of the server, as well as all exclusive
	 * tasks.
	 */
	CHECKFATAL(isc_task_create(ns_g_taskmgr, 0, &server->task),
		   "creating server task");
	isc_task_setname(server->task, "server", server);
	isc_taskmgr_setexcltask(ns_g_taskmgr, server->task);
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
	CHECKFATAL(dns_zonemgr_setsize(server->zonemgr, 1000),
		   "dns_zonemgr_setsize");

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

	server->bindkeysfile = isc_mem_strdup(server->mctx, "bind.keys");
	CHECKFATAL(server->bindkeysfile == NULL ? ISC_R_NOMEMORY :
						  ISC_R_SUCCESS,
		   "isc_mem_strdup");

	server->dumpfile = isc_mem_strdup(server->mctx, "named_dump.db");
	CHECKFATAL(server->dumpfile == NULL ? ISC_R_NOMEMORY : ISC_R_SUCCESS,
		   "isc_mem_strdup");

	server->secrootsfile = isc_mem_strdup(server->mctx, "named.secroots");
	CHECKFATAL(server->secrootsfile == NULL ? ISC_R_NOMEMORY :
						  ISC_R_SUCCESS,
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

	ISC_LIST_INIT(server->cachelist);

	server->sessionkey = NULL;
	server->session_keyfile = NULL;
	server->session_keyname = NULL;
	server->session_keyalg = DST_ALG_UNKNOWN;
	server->session_keybits = 0;

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
	isc_mem_free(server->mctx, server->bindkeysfile);
	isc_mem_free(server->mctx, server->dumpfile);
	isc_mem_free(server->mctx, server->secrootsfile);
	isc_mem_free(server->mctx, server->recfile);

	if (server->version != NULL)
		isc_mem_free(server->mctx, server->version);
	if (server->hostname != NULL)
		isc_mem_free(server->mctx, server->hostname);
	if (server->server_id != NULL)
		isc_mem_free(server->mctx, server->server_id);

	if (server->zonemgr != NULL)
		dns_zonemgr_detach(&server->zonemgr);

	if (server->tkeyctx != NULL)
		dns_tkeyctx_destroy(&server->tkeyctx);

	dst_lib_destroy();

	isc_event_free(&server->reload_event);

	INSIST(ISC_LIST_EMPTY(server->viewlist));
	INSIST(ISC_LIST_EMPTY(server->cachelist));

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
zone_from_args(ns_server_t *server, char *args, const char *zonetxt,
	       dns_zone_t **zonep, const char **zonename,
	       isc_buffer_t *text, isc_boolean_t skip)
{
	char *input, *ptr;
	char *classtxt;
	const char *viewtxt = NULL;
	dns_fixedname_t fname;
	dns_name_t *name;
	isc_result_t result;
	dns_view_t *view = NULL;
	dns_rdataclass_t rdclass;
	char problem[DNS_NAME_FORMATSIZE + 500] = "";

	REQUIRE(zonep != NULL && *zonep == NULL);
	REQUIRE(zonename == NULL || *zonename == NULL);

	input = args;

	if (skip) {
		/* Skip the command name. */
		ptr = next_token(&input, " \t");
		if (ptr == NULL)
			return (ISC_R_UNEXPECTEDEND);
	}

	/* Look for the zone name. */
	if (zonetxt == NULL)
		zonetxt = next_token(&input, " \t");
	if (zonetxt == NULL)
		return (ISC_R_SUCCESS);
	if (zonename != NULL)
		*zonename = zonetxt;

	/* Look for the optional class name. */
	classtxt = next_token(&input, " \t");
	if (classtxt != NULL) {
		/* Look for the optional view name. */
		viewtxt = next_token(&input, " \t");
	}

	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	CHECK(dns_name_fromstring(name, zonetxt, 0, NULL));

	if (classtxt != NULL) {
		isc_textregion_t r;
		r.base = classtxt;
		r.length = strlen(classtxt);
		CHECK(dns_rdataclass_fromtext(&rdclass, &r));
	} else
		rdclass = dns_rdataclass_in;

	if (viewtxt == NULL) {
		result = dns_viewlist_findzone(&server->viewlist, name,
					       ISC_TF(classtxt == NULL),
					       rdclass, zonep);
		if (result == ISC_R_NOTFOUND)
			snprintf(problem, sizeof(problem),
				 "no matching zone '%s' in any view",
				 zonetxt);
		else if (result == ISC_R_MULTIPLE)
			snprintf(problem, sizeof(problem),
				 "zone '%s' was found in multiple views",
				 zonetxt);
	} else {
		result = dns_viewlist_find(&server->viewlist, viewtxt,
					   rdclass, &view);
		if (result != ISC_R_SUCCESS) {
			snprintf(problem, sizeof(problem),
				 "no matching view '%s'", viewtxt);
			goto report;
		}

		result = dns_zt_find(view->zonetable, name, 0, NULL, zonep);
		if (result != ISC_R_SUCCESS)
			snprintf(problem, sizeof(problem),
				 "no matching zone '%s' in view '%s'",
				 zonetxt, viewtxt);
	}

	/* Partial match? */
	if (result != ISC_R_SUCCESS && *zonep != NULL)
		dns_zone_detach(zonep);
	if (result == DNS_R_PARTIALMATCH)
		result = ISC_R_NOTFOUND;
 report:
	if (result != ISC_R_SUCCESS) {
		isc_result_t tresult;

		tresult = putstr(text, problem);
		if (tresult == ISC_R_SUCCESS)
			(void) putnull(text);
	}

 cleanup:
	if (view != NULL)
		dns_view_detach(&view);

	return (result);
}

/*
 * Act on a "retransfer" command from the command channel.
 */
isc_result_t
ns_server_retransfercommand(ns_server_t *server, char *args,
			    isc_buffer_t *text)
{
	isc_result_t result;
	dns_zone_t *zone = NULL;
	dns_zone_t *raw = NULL;
	dns_zonetype_t type;

	result = zone_from_args(server, args, NULL, &zone, NULL,
				text, ISC_TRUE);
	if (result != ISC_R_SUCCESS)
		return (result);
	if (zone == NULL)
		return (ISC_R_UNEXPECTEDEND);
	dns_zone_getraw(zone, &raw);
	if (raw != NULL) {
		dns_zone_detach(&zone);
		dns_zone_attach(raw, &zone);
		dns_zone_detach(&raw);
	}
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

	result = zone_from_args(server, args, NULL, &zone, NULL,
				text, ISC_TRUE);
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
ns_server_reconfigcommand(ns_server_t *server) {
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
cleanup:
	return (result);
}

/*
 * Act on a "notify" command from the command channel.
 */
isc_result_t
ns_server_notifycommand(ns_server_t *server, char *args, isc_buffer_t *text) {
	isc_result_t result;
	dns_zone_t *zone = NULL;
	const unsigned char msg[] = "zone notify queued";

	result = zone_from_args(server, args, NULL, &zone, NULL,
				text, ISC_TRUE);
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
	dns_zone_t *zone = NULL, *raw = NULL;
	const unsigned char msg1[] = "zone refresh queued";
	const unsigned char msg2[] = "not a slave or stub zone";
	dns_zonetype_t type;

	result = zone_from_args(server, args, NULL, &zone, NULL,
				text, ISC_TRUE);
	if (result != ISC_R_SUCCESS)
		return (result);
	if (zone == NULL)
		return (ISC_R_UNEXPECTEDEND);

	dns_zone_getraw(zone, &raw);
	if (raw != NULL) {
		dns_zone_detach(&zone);
		dns_zone_attach(raw, &zone);
		dns_zone_detach(&raw);
	}

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
ns_server_togglequerylog(ns_server_t *server, char *args) {
	isc_boolean_t value;
	char *ptr;

	/* Skip the command name. */
	ptr = next_token(&args, " \t");
	if (ptr == NULL)
		return (ISC_R_UNEXPECTEDEND);

	ptr = next_token(&args, " \t");
	if (ptr == NULL)
		value = server->log_queries ? ISC_FALSE : ISC_TRUE;
	else if (strcasecmp(ptr, "yes") == 0 || strcasecmp(ptr, "on") == 0)
		value = ISC_TRUE;
	else if (strcasecmp(ptr, "no") == 0 || strcasecmp(ptr, "off") == 0)
		value = ISC_FALSE;
	else
		return (ISC_R_NOTFOUND);

	if (server->log_queries == value)
		return (ISC_R_SUCCESS);

	server->log_queries = value;

	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
		      "query logging is now %s",
		      server->log_queries ? "on" : "off");
	return (ISC_R_SUCCESS);
}

static isc_result_t
ns_listenlist_fromconfig(const cfg_obj_t *listenlist, const cfg_obj_t *config,
			 cfg_aclconfctx_t *actx, isc_mem_t *mctx,
			 isc_uint16_t family, ns_listenlist_t **target)
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
						 mctx, family, &delt);
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
			cfg_aclconfctx_t *actx, isc_mem_t *mctx,
			isc_uint16_t family, ns_listenelt_t **target)
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

	result = cfg_acl_fromconfig2(cfg_tuple_get(listener, "acl"),
				     config, ns_g_lctx, actx, mctx, 0,
				     family, &delt->acl);
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
	if (dctx->dumpcache && dns_view_iscacheshared(dctx->view->view)) {
		fprintf(dctx->fp,
			";\n; Cache of view '%s' is shared as '%s'\n",
			dctx->view->view->name,
			dns_cache_getname(dctx->view->view->cache));
	} else if (dctx->zone == NULL && dctx->cache == NULL &&
		   dctx->dumpcache)
	{
		style = &dns_master_style_cache;
		/* start cache dump */
		if (dctx->view->view->cachedb != NULL)
			dns_db_attach(dctx->view->view->cachedb, &dctx->cache);
		if (dctx->cache != NULL) {
			fprintf(dctx->fp,
				";\n; Cache dump of view '%s' (cache %s)\n;\n",
				dctx->view->view->name,
				dns_cache_getname(dctx->view->view->cache));
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

	if ((dctx->dumpadb || dctx->dumpbad) &&
	    dctx->cache == NULL && dctx->view->view->cachedb != NULL)
		dns_db_attach(dctx->view->view->cachedb, &dctx->cache);

	if (dctx->cache != NULL) {
		if (dctx->dumpadb)
			dns_adb_dump(dctx->view->view->adb, dctx->fp);
		if (dctx->dumpbad)
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
				POST(result);
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
	dctx->dumpadb = ISC_TRUE;
	dctx->dumpbad = ISC_TRUE;
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
		/* also dump zones */
		dctx->dumpzones = ISC_TRUE;
		ptr = next_token(&args, " \t");
	} else if (ptr != NULL && strcmp(ptr, "-cache") == 0) {
		/* this is the default */
		ptr = next_token(&args, " \t");
	} else if (ptr != NULL && strcmp(ptr, "-zones") == 0) {
		/* only dump zones, suppress caches */
		dctx->dumpadb = ISC_FALSE;
		dctx->dumpbad = ISC_FALSE;
		dctx->dumpcache = ISC_FALSE;
		dctx->dumpzones = ISC_TRUE;
		ptr = next_token(&args, " \t");
#ifdef ENABLE_FETCHLIMIT
	} else if (ptr != NULL && strcmp(ptr, "-adb") == 0) {
		/* only dump adb, suppress other caches */
		dctx->dumpbad = ISC_FALSE;
		dctx->dumpcache = ISC_FALSE;
		ptr = next_token(&args, " \t");
	} else if (ptr != NULL && strcmp(ptr, "-bad") == 0) {
		/* only dump badcache, suppress other caches */
		dctx->dumpadb = ISC_FALSE;
		dctx->dumpcache = ISC_FALSE;
		ptr = next_token(&args, " \t");
#endif /* ENABLE_FETCHLIMIT */
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
ns_server_dumpsecroots(ns_server_t *server, char *args) {
	dns_view_t *view;
	dns_keytable_t *secroots = NULL;
	isc_result_t result;
	char *ptr;
	FILE *fp = NULL;
	isc_time_t now;
	char tbuf[64];

	/* Skip the command name. */
	ptr = next_token(&args, " \t");
	if (ptr == NULL)
		return (ISC_R_UNEXPECTEDEND);

	ptr = next_token(&args, " \t");

	CHECKMF(isc_stdio_open(server->secrootsfile, "w", &fp),
		"could not open secroots dump file", server->secrootsfile);
	TIME_NOW(&now);
	isc_time_formattimestamp(&now, tbuf, sizeof(tbuf));
	fprintf(fp, "%s\n", tbuf);

	do {
		for (view = ISC_LIST_HEAD(server->viewlist);
		     view != NULL;
		     view = ISC_LIST_NEXT(view, link))
		{
			if (ptr != NULL && strcmp(view->name, ptr) != 0)
				continue;
			if (secroots != NULL)
				dns_keytable_detach(&secroots);
			result = dns_view_getsecroots(view, &secroots);
			if (result == ISC_R_NOTFOUND) {
				result = ISC_R_SUCCESS;
				continue;
			}
			fprintf(fp, "\n Start view %s\n\n", view->name);
			result = dns_keytable_dump(secroots, fp);
			if (result != ISC_R_SUCCESS)
				fprintf(fp, " dumpsecroots failed: %s\n",
					isc_result_totext(result));
		}
		if (ptr != NULL)
			ptr = next_token(&args, " \t");
	} while (ptr != NULL);

 cleanup:
	if (secroots != NULL)
		dns_keytable_detach(&secroots);
	if (fp != NULL)
		(void)isc_stdio_close(fp);
	if (result == ISC_R_SUCCESS)
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
			      "dumpsecroots complete");
	else
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "dumpsecroots failed: %s",
			      dns_result_totext(result));
	return (result);
}

isc_result_t
ns_server_dumprecursing(ns_server_t *server) {
	FILE *fp = NULL;
	isc_result_t result;
#ifdef ENABLE_FETCHLIMIT
	dns_view_t *view;
#endif /* ENABLE_FETCHLIMIT */

	CHECKMF(isc_stdio_open(server->recfile, "w", &fp),
		"could not open dump file", server->recfile);
	fprintf(fp, ";\n; Recursing Queries\n;\n");
	ns_interfacemgr_dumprecursing(fp, server->interfacemgr);

#ifdef ENABLE_FETCHLIMIT
	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link))
	{
		fprintf(fp, ";\n; Active fetch domains [view: %s]\n;\n",
			view->name);
		dns_resolver_dumpfetches(view->resolver, fp);
	}
#endif /* ENABLE_FETCHLIMIT */

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
			goto cleanup;
		view->enablevalidation = enable;
		changed = ISC_TRUE;
	}
	if (changed)
		result = ISC_R_SUCCESS;
	else
		result = ISC_R_FAILURE;
 cleanup:
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
	ns_cache_t *nsc;

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

	/*
	 * Flushing a cache is tricky when caches are shared by multiple views.
	 * We first identify which caches should be flushed in the local cache
	 * list, flush these caches, and then update other views that refer to
	 * the flushed cache DB.
	 */
	if (viewname != NULL) {
		/*
		 * Mark caches that need to be flushed.  This is an O(#view^2)
		 * operation in the very worst case, but should be normally
		 * much more lightweight because only a few (most typically just
		 * one) views will match.
		 */
		for (view = ISC_LIST_HEAD(server->viewlist);
		     view != NULL;
		     view = ISC_LIST_NEXT(view, link))
		{
			if (strcasecmp(viewname, view->name) != 0)
				continue;
			found = ISC_TRUE;
			for (nsc = ISC_LIST_HEAD(server->cachelist);
			     nsc != NULL;
			     nsc = ISC_LIST_NEXT(nsc, link)) {
				if (nsc->cache == view->cache)
					break;
			}
			INSIST(nsc != NULL);
			nsc->needflush = ISC_TRUE;
		}
	} else
		found = ISC_TRUE;

	/* Perform flush */
	for (nsc = ISC_LIST_HEAD(server->cachelist);
	     nsc != NULL;
	     nsc = ISC_LIST_NEXT(nsc, link)) {
		if (viewname != NULL && !nsc->needflush)
			continue;
		nsc->needflush = ISC_TRUE;
		result = dns_view_flushcache2(nsc->primaryview, ISC_FALSE);
		if (result != ISC_R_SUCCESS) {
			flushed = ISC_FALSE;
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
				      "flushing cache in view '%s' failed: %s",
				      nsc->primaryview->name,
				      isc_result_totext(result));
		}
	}

	/*
	 * Fix up views that share a flushed cache: let the views update the
	 * cache DB they're referring to.  This could also be an expensive
	 * operation, but should typically be marginal: the inner loop is only
	 * necessary for views that share a cache, and if there are many such
	 * views the number of shared cache should normally be small.
	 * A worst case is that we have n views and n/2 caches, each shared by
	 * two views.  Then this will be a O(n^2/4) operation.
	 */
	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link))
	{
		if (!dns_view_iscacheshared(view))
			continue;
		for (nsc = ISC_LIST_HEAD(server->cachelist);
		     nsc != NULL;
		     nsc = ISC_LIST_NEXT(nsc, link)) {
			if (!nsc->needflush || nsc->cache != view->cache)
				continue;
			result = dns_view_flushcache2(view, ISC_TRUE);
			if (result != ISC_R_SUCCESS) {
				flushed = ISC_FALSE;
				isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
					      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
					      "fixing cache in view '%s' "
					      "failed: %s", view->name,
					      isc_result_totext(result));
			}
		}
	}

	/* Cleanup the cache list. */
	for (nsc = ISC_LIST_HEAD(server->cachelist);
	     nsc != NULL;
	     nsc = ISC_LIST_NEXT(nsc, link)) {
		nsc->needflush = ISC_FALSE;
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
ns_server_flushnode(ns_server_t *server, char *args, isc_boolean_t tree) {
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

	isc_buffer_constinit(&b, target, strlen(target));
	isc_buffer_add(&b, strlen(target));
	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	result = dns_name_fromtext(name, &b, dns_rootname, 0, NULL);
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
		/*
		 * It's a little inefficient to try flushing name for all views
		 * if some of the views share a single cache.  But since the
		 * operation is lightweight we prefer simplicity here.
		 */
		result = dns_view_flushnode(view, name, tree);
		if (result != ISC_R_SUCCESS) {
			flushed = ISC_FALSE;
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
				      "flushing %s '%s' in cache view '%s' "
				      "failed: %s",
				      tree ? "tree" : "name",
				      target, view->name,
				      isc_result_totext(result));
		}
	}
	if (flushed && found) {
		if (viewname != NULL)
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
				      "flushing %s '%s' in cache view '%s' "
				      "succeeded",
				      tree ? "tree" : "name",
				      target, viewname);
		else
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
				      "flushing %s '%s' in all cache views "
				      "succeeded",
				      tree ? "tree" : "name",
				      target);
		result = ISC_R_SUCCESS;
	} else {
		if (!found)
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
				      "flushing %s '%s' in cache view '%s' "
				      "failed: view not found",
				      tree ? "tree" : "name",
				      target, viewname);
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
		     "version: %s %s%s%s <id:%s>%s%s%s\n"
#ifdef ISC_PLATFORM_USETHREADS
		     "CPUs found: %u\n"
		     "worker threads: %u\n"
		     "UDP listeners per interface: %u\n"
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
		     ns_g_product, ns_g_version,
		     (*ns_g_description != '\0') ? " " : "",
		     ns_g_description, ns_g_srcid, ob, alt, cb,
#ifdef ISC_PLATFORM_USETHREADS
		     ns_g_cpus_detected, ns_g_cpus, ns_g_udpdisp,
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
				if (*foundkeys != 0)
					CHECK(putstr(text, "\n"));
				CHECK(putstr(text, "view \""));
				CHECK(putstr(text, viewname));
				CHECK(putstr(text,
					     "\"; type \"dynamic\"; key \""));
				CHECK(putstr(text, namestr));
				CHECK(putstr(text, "\"; creator \""));
				CHECK(putstr(text, creatorstr));
				CHECK(putstr(text, "\";"));
			} else {
				if (*foundkeys != 0)
					CHECK(putstr(text, "\n"));
				CHECK(putstr(text, "view \""));
				CHECK(putstr(text, viewname));
				CHECK(putstr(text,
					     "\"; type \"static\"; key \""));
				CHECK(putstr(text, namestr));
				CHECK(putstr(text, "\";"));
			}
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

cleanup:
	return (result);
}

isc_result_t
ns_server_tsiglist(ns_server_t *server, isc_buffer_t *text) {
	isc_result_t result;
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

	if (foundkeys == 0)
		CHECK(putstr(text, "no tsig keys found."));

	if (isc_buffer_usedlength(text) > 0)
		CHECK(putnull(text));

	return (ISC_R_SUCCESS);

 cleanup:
	return (result);
}

/*
 * Act on a "sign" or "loadkeys" command from the command channel.
 */
isc_result_t
ns_server_rekey(ns_server_t *server, char *args, isc_buffer_t *text) {
	isc_result_t result;
	dns_zone_t *zone = NULL;
	dns_zonetype_t type;
	isc_uint16_t keyopts;
	isc_boolean_t fullsign = ISC_FALSE;

	if (strncasecmp(args, NS_COMMAND_SIGN, strlen(NS_COMMAND_SIGN)) == 0)
	    fullsign = ISC_TRUE;

	result = zone_from_args(server, args, NULL, &zone, NULL,
				text, ISC_TRUE);
	if (result != ISC_R_SUCCESS)
		return (result);
	if (zone == NULL)
		return (ISC_R_UNEXPECTEDEND);   /* XXX: or do all zones? */

	type = dns_zone_gettype(zone);
	if (type != dns_zone_master) {
		dns_zone_detach(&zone);
		return (DNS_R_NOTMASTER);
	}

	keyopts = dns_zone_getkeyopts(zone);

	/* "rndc loadkeys" requires "auto-dnssec maintain". */
	if ((keyopts & DNS_ZONEKEY_ALLOW) == 0)
		result = ISC_R_NOPERM;
	else if ((keyopts & DNS_ZONEKEY_MAINTAIN) == 0 && !fullsign)
		result = ISC_R_NOPERM;
	else
		dns_zone_rekey(zone, fullsign);

	dns_zone_detach(&zone);
	return (result);
}

/*
 * Act on a "sync" command from the command channel.
*/
static isc_result_t
synczone(dns_zone_t *zone, void *uap) {
	isc_boolean_t cleanup = *(isc_boolean_t *)uap;
	isc_result_t result;
	dns_zone_t *raw = NULL;
	char *journal;

	dns_zone_getraw(zone, &raw);
	if (raw != NULL) {
		synczone(raw, uap);
		dns_zone_detach(&raw);
	}

	result = dns_zone_flush(zone);
	if (result != ISC_R_SUCCESS)
		cleanup = ISC_FALSE;
	if (cleanup) {
		journal = dns_zone_getjournal(zone);
		if (journal != NULL)
			(void)isc_file_remove(journal);
	}

	return (result);
}

isc_result_t
ns_server_sync(ns_server_t *server, char *args, isc_buffer_t *text) {
	isc_result_t result, tresult;
	dns_view_t *view;
	dns_zone_t *zone = NULL;
	char classstr[DNS_RDATACLASS_FORMATSIZE];
	char zonename[DNS_NAME_FORMATSIZE];
	const char *vname, *sep, *msg = NULL, *arg;
	isc_boolean_t cleanup = ISC_FALSE;

	(void) next_token(&args, " \t");

	arg = next_token(&args, " \t");
	if (arg != NULL &&
	    (strcmp(arg, "-clean") == 0 || strcmp(arg, "-clear") == 0)) {
		cleanup = ISC_TRUE;
		arg = next_token(&args, " \t");
	}

	result = zone_from_args(server, args, arg, &zone, NULL,
				text, ISC_FALSE);
	if (result != ISC_R_SUCCESS)
		return (result);

	if (zone == NULL) {
		result = isc_task_beginexclusive(server->task);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		tresult = ISC_R_SUCCESS;
		for (view = ISC_LIST_HEAD(server->viewlist);
		     view != NULL;
		     view = ISC_LIST_NEXT(view, link)) {
			result = dns_zt_apply(view->zonetable, ISC_FALSE,
					      synczone, &cleanup);
			if (result != ISC_R_SUCCESS &&
			    tresult == ISC_R_SUCCESS)
				tresult = result;
		}
		isc_task_endexclusive(server->task);
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
			      "dumping all zones%s: %s",
			      cleanup ? ", removing journal files" : "",
			      isc_result_totext(result));
		return (tresult);
	}

	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	result = synczone(zone, &cleanup);
	isc_task_endexclusive(server->task);

	if (msg != NULL && strlen(msg) < isc_buffer_availablelength(text))
		isc_buffer_putmem(text, (const unsigned char *)msg,
				  strlen(msg) + 1);

	view = dns_zone_getview(zone);
	if (strcmp(view->name, "_default") == 0 ||
	    strcmp(view->name, "_bind") == 0)
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
		      "sync: dumping zone '%s/%s'%s%s%s: %s",
		      zonename, classstr, sep, vname,
		      cleanup ? ", removing journal file" : "",
		      isc_result_totext(result));
	dns_zone_detach(&zone);
	return (result);
}

/*
 * Act on a "freeze" or "thaw" command from the command channel.
 */
isc_result_t
ns_server_freeze(ns_server_t *server, isc_boolean_t freeze, char *args,
		 isc_buffer_t *text)
{
	isc_result_t result, tresult;
	dns_zone_t *zone = NULL, *raw = NULL;
	dns_zonetype_t type;
	char classstr[DNS_RDATACLASS_FORMATSIZE];
	char zonename[DNS_NAME_FORMATSIZE];
	dns_view_t *view;
	const char *vname, *sep;
	isc_boolean_t frozen;
	const char *msg = NULL;

	result = zone_from_args(server, args, NULL, &zone, NULL,
				text, ISC_TRUE);
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
	dns_zone_getraw(zone, &raw);
	if (raw != NULL) {
		dns_zone_detach(&zone);
		dns_zone_attach(raw, &zone);
		dns_zone_detach(&raw);
	}
	type = dns_zone_gettype(zone);
	if (type != dns_zone_master) {
		dns_zone_detach(&zone);
		return (DNS_R_NOTMASTER);
	}

	if (freeze && !dns_zone_isdynamic(zone, ISC_TRUE)) {
		dns_zone_detach(&zone);
		return (DNS_R_NOTDYNAMIC);
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
	if (strcmp(view->name, "_default") == 0 ||
	    strcmp(view->name, "_bind") == 0)
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

/*
 * Emit a comment at the top of the nzf file containing the viewname
 * Expects the fp to already be open for writing
 */
#define HEADER1 "# New zone file for view: "
#define HEADER2 "\n# This file contains configuration for zones added by\n" \
		"# the 'rndc addzone' command. DO NOT EDIT BY HAND.\n"
isc_result_t
add_comment(FILE *fp, const char *viewname) {
	isc_result_t result;
	CHECK(isc_stdio_write(HEADER1, sizeof(HEADER1) - 1, 1, fp, NULL));
	CHECK(isc_stdio_write(viewname, strlen(viewname), 1, fp, NULL));
	CHECK(isc_stdio_write(HEADER2, sizeof(HEADER2) - 1, 1, fp, NULL));
 cleanup:
	return (result);
}

/*
 * Act on an "addzone" command from the command channel.
 */
isc_result_t
ns_server_add_zone(ns_server_t *server, char *args, isc_buffer_t *text) {
	isc_result_t	     result, tresult;
	isc_buffer_t	     argbuf;
	size_t		     arglen;
	cfg_parser_t	    *parser = NULL;
	cfg_obj_t	    *config = NULL;
	const cfg_obj_t	    *vconfig = NULL;
	const cfg_obj_t	    *views = NULL;
	const cfg_obj_t     *parms = NULL;
	const cfg_obj_t     *obj = NULL;
	const cfg_listelt_t *element;
	const char	    *zonename;
	const char	    *classname = NULL;
	const char	    *argp;
	const char	    *viewname = NULL;
	dns_rdataclass_t     rdclass;
	dns_view_t	    *view = NULL;
	isc_buffer_t	     buf;
	dns_fixedname_t	     fname;
	dns_name_t	    *dnsname;
	dns_zone_t	    *zone = NULL;
	FILE		    *fp = NULL;
	struct cfg_context  *cfg = NULL;
	char 		    namebuf[DNS_NAME_FORMATSIZE];
	off_t		    offset;

	/* Try to parse the argument string */
	arglen = strlen(args);
	isc_buffer_init(&argbuf, args, (unsigned int)arglen);
	isc_buffer_add(&argbuf, strlen(args));
	CHECK(cfg_parser_create(server->mctx, ns_g_lctx, &parser));
	CHECK(cfg_parse_buffer(parser, &argbuf, &cfg_type_addzoneconf,
			       &config));
	CHECK(cfg_map_get(config, "addzone", &parms));

	zonename = cfg_obj_asstring(cfg_tuple_get(parms, "name"));
	isc_buffer_constinit(&buf, zonename, strlen(zonename));
	isc_buffer_add(&buf, strlen(zonename));

	dns_fixedname_init(&fname);
	dnsname = dns_fixedname_name(&fname);
	CHECK(dns_name_fromtext(dnsname, &buf, dns_rootname, ISC_FALSE, NULL));

	/* Make sense of optional class argument */
	obj = cfg_tuple_get(parms, "class");
	CHECK(ns_config_getclass(obj, dns_rdataclass_in, &rdclass));
	if (rdclass != dns_rdataclass_in && obj)
		classname = cfg_obj_asstring(obj);

	/* Make sense of optional view argument */
	obj = cfg_tuple_get(parms, "view");
	if (obj && cfg_obj_isstring(obj))
		viewname = cfg_obj_asstring(obj);
	if (viewname == NULL || *viewname == '\0')
		viewname = "_default";
	CHECK(dns_viewlist_find(&server->viewlist, viewname, rdclass, &view));

	/* Are we accepting new zones? */
	if (view->new_zone_file == NULL) {
		result = ISC_R_NOPERM;
		goto cleanup;
	}

	cfg = (struct cfg_context *) view->new_zone_config;
	if (cfg == NULL) {
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	/* Zone shouldn't already exist */
	result = dns_zt_find(view->zonetable, dnsname, 0, NULL, &zone);
	if (result == ISC_R_SUCCESS) {
		result = ISC_R_EXISTS;
		goto cleanup;
	} else if (result == DNS_R_PARTIALMATCH) {
		/* Create our sub-zone anyway */
		dns_zone_detach(&zone);
		zone = NULL;
	}
	else if (result != ISC_R_NOTFOUND)
		goto cleanup;

	/* Find the view statement */
	cfg_map_get(cfg->config, "view", &views);
	for (element = cfg_list_first(views);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const char *vname;
		vconfig = cfg_listelt_value(element);
		vname = cfg_obj_asstring(cfg_tuple_get(vconfig, "name"));
		if (vname && !strcasecmp(vname, viewname))
			break;
		vconfig = NULL;
	}

	/* Open save file for write configuration */
	result = isc_stdio_open(view->new_zone_file, "a", &fp);
	if (result != ISC_R_SUCCESS) {
		TCHECK(putstr(text, "unable to open '"));
		TCHECK(putstr(text, view->new_zone_file));
		TCHECK(putstr(text, "': "));
		TCHECK(putstr(text, isc_result_totext(result)));
		goto cleanup;
	}
	CHECK(isc_stdio_tell(fp, &offset));
	if (offset == 0)
		CHECK(add_comment(fp, view->name));

	/* Mark view unfrozen so that zone can be added */
	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	dns_view_thaw(view);
	result = configure_zone(cfg->config, parms, vconfig,
				server->mctx, view, cfg->actx, ISC_FALSE);
	dns_view_freeze(view);
	isc_task_endexclusive(server->task);
	if (result != ISC_R_SUCCESS) {
		TCHECK(putstr(text, "configure_zone failed: "));
		TCHECK(putstr(text, isc_result_totext(result)));
		goto cleanup;
	}

	/* Is it there yet? */
	CHECK(dns_zt_find(view->zonetable, dnsname, 0, NULL, &zone));

	/*
	 * Load the zone from the master file.  If this fails, we'll
	 * need to undo the configuration we've done already.
	 */
	result = dns_zone_loadnew(zone);
	if (result != ISC_R_SUCCESS) {
		dns_db_t *dbp = NULL;

		TCHECK(putstr(text, "dns_zone_loadnew failed: "));
		TCHECK(putstr(text, isc_result_totext(result)));

		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
			      "addzone failed; reverting.");

		/* If the zone loaded partially, unload it */
		if (dns_zone_getdb(zone, &dbp) == ISC_R_SUCCESS) {
			dns_db_detach(&dbp);
			dns_zone_unload(zone);
		}

		/* Remove the zone from the zone table */
		dns_zt_unmount(view->zonetable, zone);
		goto cleanup;
	}

	/* Flag the zone as having been added at runtime */
	dns_zone_setadded(zone, ISC_TRUE);

	/* Emit the zone name, quoted and escaped */
	isc_buffer_init(&buf, namebuf, sizeof(namebuf));
	CHECK(dns_name_totext(dnsname, ISC_TRUE, &buf));
	putnull(&buf);
	CHECK(isc_stdio_write("zone \"", 6, 1, fp, NULL));
	CHECK(isc_stdio_write(namebuf, strlen(namebuf), 1, fp, NULL));
	CHECK(isc_stdio_write("\" ", 2, 1, fp, NULL));

	/* Classname, if not default */
	if (classname != NULL && *classname != '\0') {
		CHECK(isc_stdio_write(classname, strlen(classname), 1, fp,
				      NULL));
		CHECK(isc_stdio_write(" ", 1, 1, fp, NULL));
	}

	/* Find beginning of option block from args */
	for (argp = args; *argp; argp++, arglen--) {
		if (*argp == '{') {	/* Assume matching '}' */
			/* Add that to our file */
			CHECK(isc_stdio_write(argp, arglen, 1, fp, NULL));

			/* Make sure we end with a LF */
			if (argp[arglen-1] != '\n') {
				CHECK(isc_stdio_write("\n", 1, 1, fp, NULL));
			}
			break;
		}
	}

	CHECK(isc_stdio_close(fp));
	fp = NULL;
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				  NS_LOGMODULE_SERVER, ISC_LOG_INFO,
				  "zone %s added to view %s via addzone",
				  zonename, viewname);

	result = ISC_R_SUCCESS;

 cleanup:
	if (isc_buffer_usedlength(text) > 0)
		putnull(text);
	if (fp != NULL)
		isc_stdio_close(fp);
	if (parser != NULL) {
		if (config != NULL)
			cfg_obj_destroy(parser, &config);
		cfg_parser_destroy(&parser);
	}
	if (zone != NULL)
		dns_zone_detach(&zone);
	if (view != NULL)
		dns_view_detach(&view);

	return (result);
}

/*
 * Act on a "delzone" command from the command channel.
 */
isc_result_t
ns_server_del_zone(ns_server_t *server, char *args, isc_buffer_t *text) {
	isc_result_t result;
	dns_zone_t *zone = NULL;
	dns_view_t *view = NULL;
	dns_db_t *dbp = NULL;
	const char *filename = NULL;
	char *tmpname = NULL;
	char buf[1024];
	const char *zonename = NULL;
	size_t znamelen = 0;
	FILE *ifp = NULL, *ofp = NULL;
	isc_boolean_t inheader = ISC_TRUE;

	/* Parse parameters */
	CHECK(zone_from_args(server, args, NULL, &zone, &zonename,
			     text, ISC_TRUE));

	if (zone == NULL) {
		result = ISC_R_UNEXPECTEDEND;
		goto cleanup;
	}

	/*
	 * Was this zone originally added at runtime?
	 * If not, we can't delete it now.
	 */
	if (!dns_zone_getadded(zone)) {
		result = ISC_R_NOPERM;
		goto cleanup;
	}

	INSIST(zonename != NULL);
	znamelen = strlen(zonename);

	/* Dig out configuration for this zone */
	view = dns_zone_getview(zone);
	filename = view->new_zone_file;
	if (filename == NULL) {
		/* No adding zones in this view */
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	/* Rewrite zone list */
	result = isc_stdio_open(filename, "r", &ifp);
	if (ifp != NULL && result == ISC_R_SUCCESS) {
		char *found = NULL, *p = NULL;
		size_t n;

		/* Create a temporary file */
		CHECK(isc_string_printf(buf, 1023, "%s.%ld", filename,
					(long)getpid()));
		if (!(tmpname = isc_mem_strdup(server->mctx, buf))) {
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
		CHECK(isc_stdio_open(tmpname, "w", &ofp));
		CHECK(add_comment(ofp, view->name));

		/* Look for the entry for that zone */
		while (fgets(buf, 1024, ifp)) {
			/* Skip initial comment, if any */
			if (inheader && *buf == '#')
				continue;
			if (*buf != '#')
				inheader = ISC_FALSE;

			/*
			 * Any other lines not starting with zone, copy
			 * them out and continue.
			 */
			if (strncasecmp(buf, "zone", 4) != 0) {
				fputs(buf, ofp);
				continue;
			}
			p = buf+4;

			/* This is a zone; find its name. */
			while (*p &&
			       ((*p == '"') || isspace((unsigned char)*p)))
				p++;

			/*
			 * If it's not the zone we're looking for, copy
			 * it out and continue
			 */
			if (strncasecmp(p, zonename, znamelen) != 0) {
				fputs(buf, ofp);
				continue;
			}

			/*
			 * But if it is the zone we want, skip over it
			 * so it will be omitted from the new file
			 */
			p += znamelen;
			if (isspace((unsigned char)*p) ||
			    *p == '"' || *p == '{') {
				/* This must be the entry */
				found = p;
				break;
			}

			/* Copy the rest of the buffer out and continue */
			fputs(buf, ofp);
		}

		/* Skip over an option block (matching # of braces) */
		if (found) {
			int obrace = 0, cbrace = 0;
			for (;;) {
				while (*p) {
					if (*p == '{') obrace++;
					if (*p == '}') cbrace++;
					p++;
				}
				if (obrace && (obrace == cbrace))
					break;
				if (!fgets(buf, 1024, ifp))
					break;
				p = buf;
			}

			/* Just spool the remainder of the file out */
			result = isc_stdio_read(buf, 1, 1024, ifp, &n);
			while (n > 0U) {
				if (result == ISC_R_EOF)
					result = ISC_R_SUCCESS;
				CHECK(result);
				isc_stdio_write(buf, 1, n, ofp, NULL);
				result = isc_stdio_read(buf, 1, 1024, ifp, &n);
			}

			/*
			 * Close files before overwriting the nzfile
			 * with the temporary file as it's necessary on
			 * some platforms (win32).
			 */
			(void) isc_stdio_close(ifp);
			ifp = NULL;
			(void) isc_stdio_close(ofp);
			ofp = NULL;

			/* Move temporary into place */
			CHECK(isc_file_rename(tmpname, view->new_zone_file));
		} else {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_WARNING,
				      "deleted zone %s was missing from "
				      "new zone file", zonename);
			goto cleanup;
		}
	}

	/* Stop answering for this zone */
	if (dns_zone_getdb(zone, &dbp) == ISC_R_SUCCESS) {
		dns_db_detach(&dbp);
		dns_zone_unload(zone);
	}

	CHECK(dns_zt_unmount(view->zonetable, zone));

	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				  NS_LOGMODULE_SERVER, ISC_LOG_INFO,
				  "zone %s removed via delzone", zonename);

	result = ISC_R_SUCCESS;

 cleanup:
	if (isc_buffer_usedlength(text) > 0)
		putnull(text);
	if (ifp != NULL)
		isc_stdio_close(ifp);
	if (ofp != NULL)
		isc_stdio_close(ofp);
	if (tmpname != NULL) {
		isc_file_remove(tmpname);
		isc_mem_free(server->mctx, tmpname);
	}
	if (zone != NULL)
		dns_zone_detach(&zone);

	return (result);
}

static void
newzone_cfgctx_destroy(void **cfgp) {
	struct cfg_context *cfg;

	REQUIRE(cfgp != NULL && *cfgp != NULL);

	cfg = *cfgp;

	if (cfg->actx != NULL)
		cfg_aclconfctx_detach(&cfg->actx);

	if (cfg->parser != NULL) {
		if (cfg->config != NULL)
			cfg_obj_destroy(cfg->parser, &cfg->config);
		cfg_parser_destroy(&cfg->parser);
	}
	if (cfg->nzparser != NULL) {
		if (cfg->nzconfig != NULL)
			cfg_obj_destroy(cfg->nzparser, &cfg->nzconfig);
		cfg_parser_destroy(&cfg->nzparser);
	}

	isc_mem_putanddetach(&cfg->mctx, cfg, sizeof(*cfg));
	*cfgp = NULL;
}

isc_result_t
ns_server_signing(ns_server_t *server, char *args, isc_buffer_t *text) {
	isc_result_t result = ISC_R_SUCCESS;
	dns_zone_t *zone = NULL;
	dns_name_t *origin;
	dns_db_t *db = NULL;
	dns_dbnode_t *node = NULL;
	dns_dbversion_t *version = NULL;
	dns_rdatatype_t privatetype;
	dns_rdataset_t privset;
	isc_boolean_t first = ISC_TRUE;
	isc_boolean_t list = ISC_FALSE, clear = ISC_FALSE;
	isc_boolean_t chain = ISC_FALSE;
	char keystr[DNS_SECALG_FORMATSIZE + 7]; /* <5-digit keyid>/<alg> */
	unsigned short hash = 0, flags = 0, iter = 0, saltlen = 0;
	unsigned char salt[255];
	const char *ptr;
	size_t n;

	dns_rdataset_init(&privset);

	/* Skip the command name. */
	ptr = next_token(&args, " \t");
	if (ptr == NULL)
		return (ISC_R_UNEXPECTEDEND);

	/* Find out what we are to do. */
	ptr = next_token(&args, " \t");
	if (ptr == NULL)
		return (ISC_R_UNEXPECTEDEND);

	if (strcasecmp(ptr, "-list") == 0)
		list = ISC_TRUE;
	else if ((strcasecmp(ptr, "-clear") == 0)  ||
		 (strcasecmp(ptr, "-clean") == 0)) {
		clear = ISC_TRUE;
		ptr = next_token(&args, " \t");
		if (ptr == NULL)
			return (ISC_R_UNEXPECTEDEND);
		strlcpy(keystr, ptr, sizeof(keystr));
	} else if (strcasecmp(ptr, "-nsec3param") == 0) {
		const char *hashstr, *flagstr, *iterstr;
		char nbuf[512];

		chain = ISC_TRUE;
		hashstr = next_token(&args, " \t");
		if (hashstr == NULL)
			return (ISC_R_UNEXPECTEDEND);

		if (strcasecmp(hashstr, "none") == 0)
			hash = 0;
		else {
			flagstr = next_token(&args, " \t");
			iterstr = next_token(&args, " \t");
			if (flagstr == NULL || iterstr == NULL)
				return (ISC_R_UNEXPECTEDEND);

			n = snprintf(nbuf, sizeof(nbuf), "%s %s %s",
				     hashstr, flagstr, iterstr);
			if (n == sizeof(nbuf))
				return (ISC_R_NOSPACE);
			n = sscanf(nbuf, "%hu %hu %hu", &hash, &flags, &iter);
			if (n != 3U)
				return (ISC_R_BADNUMBER);

			if (hash > 0xffU || flags > 0xffU)
				return (ISC_R_RANGE);

			ptr = next_token(&args, " \t");
			if (ptr == NULL)
				return (ISC_R_UNEXPECTEDEND);
			if (strcmp(ptr, "-") != 0) {
				isc_buffer_t buf;

				isc_buffer_init(&buf, salt, sizeof(salt));
				CHECK(isc_hex_decodestring(ptr, &buf));
				saltlen = isc_buffer_usedlength(&buf);
			}
		}
	} else
		CHECK(DNS_R_SYNTAX);

	CHECK(zone_from_args(server, args, NULL, &zone, NULL,
			     text, ISC_FALSE));
	if (zone == NULL)
		CHECK(ISC_R_UNEXPECTEDEND);

	if (clear) {
		CHECK(dns_zone_keydone(zone, keystr));
		putstr(text, "request queued");
		putnull(text);
	} else if (chain) {
		CHECK(dns_zone_setnsec3param(zone, (isc_uint8_t)hash,
					     (isc_uint8_t)flags, iter,
					     (isc_uint8_t)saltlen, salt,
					     ISC_TRUE));
		putstr(text, "request queued");
		putnull(text);
	} else if (list) {
		privatetype = dns_zone_getprivatetype(zone);
		origin = dns_zone_getorigin(zone);
		CHECK(dns_zone_getdb(zone, &db));
		CHECK(dns_db_findnode(db, origin, ISC_FALSE, &node));
		dns_db_currentversion(db, &version);

		result = dns_db_findrdataset(db, node, version, privatetype,
					     dns_rdatatype_none, 0,
					     &privset, NULL);
		if (result == ISC_R_NOTFOUND) {
			putstr(text, "No signing records found");
			putnull(text);
			result = ISC_R_SUCCESS;
			goto cleanup;
		}

		for (result = dns_rdataset_first(&privset);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(&privset))
		{
			dns_rdata_t priv = DNS_RDATA_INIT;
			char output[BUFSIZ];
			isc_buffer_t buf;

			dns_rdataset_current(&privset, &priv);

			isc_buffer_init(&buf, output, sizeof(output));
			CHECK(dns_private_totext(&priv, &buf));

			if (!first)
				putstr(text, "\n");
			first = ISC_FALSE;

			n = snprintf((char *)isc_buffer_used(text),
				     isc_buffer_availablelength(text),
				     "%s", output);
			if (n >= isc_buffer_availablelength(text))
				CHECK(ISC_R_NOSPACE);

			isc_buffer_add(text, (unsigned int)n);
		}
		if (!first)
			putnull(text);

		if (result == ISC_R_NOMORE)
			result = ISC_R_SUCCESS;
	}

 cleanup:
	if (dns_rdataset_isassociated(&privset))
		dns_rdataset_disassociate(&privset);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (version != NULL)
		dns_db_closeversion(db, &version, ISC_FALSE);
	if (db != NULL)
		dns_db_detach(&db);
	if (zone != NULL)
		dns_zone_detach(&zone);

	return (result);
}

static isc_result_t
putstr(isc_buffer_t *b, const char *str) {
	unsigned int l = strlen(str);

	/*
	 * Use >= to leave space for NUL termination.
	 */
	if (l >= isc_buffer_availablelength(b))
		return (ISC_R_NOSPACE);

	isc_buffer_putmem(b, (const unsigned char *)str, l);
	return (ISC_R_SUCCESS);
}

static isc_result_t
putnull(isc_buffer_t *b) {
	if (isc_buffer_availablelength(b) == 0)
		return (ISC_R_NOSPACE);

	isc_buffer_putuint8(b, 0);
	return (ISC_R_SUCCESS);
}
