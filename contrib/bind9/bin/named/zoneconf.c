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

/* $Id: zoneconf.c,v 1.170 2011-01-06 23:47:00 tbox Exp $ */

/*% */

#include <config.h>

#include <isc/buffer.h>
#include <isc/file.h>
#include <isc/mem.h>
#include <isc/print.h>
#include <isc/stats.h>
#include <isc/string.h>		/* Required for HP/UX (and others?) */
#include <isc/util.h>

#include <dns/acl.h>
#include <dns/db.h>
#include <dns/fixedname.h>
#include <dns/log.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdatatype.h>
#include <dns/rdataset.h>
#include <dns/rdatalist.h>
#include <dns/result.h>
#include <dns/sdlz.h>
#include <dns/ssu.h>
#include <dns/stats.h>
#include <dns/view.h>
#include <dns/zone.h>

#include <named/client.h>
#include <named/config.h>
#include <named/globals.h>
#include <named/log.h>
#include <named/server.h>
#include <named/zoneconf.h>

/* ACLs associated with zone */
typedef enum {
	allow_notify,
	allow_query,
	allow_transfer,
	allow_update,
	allow_update_forwarding
} acl_type_t;

#define RETERR(x) do { \
	isc_result_t _r = (x); \
	if (_r != ISC_R_SUCCESS) \
		return (_r); \
	} while (0)

#define CHECK(x) do { \
	result = (x); \
	if (result != ISC_R_SUCCESS) \
		goto cleanup; \
	} while (0)

/*%
 * Convenience function for configuring a single zone ACL.
 */
static isc_result_t
configure_zone_acl(const cfg_obj_t *zconfig, const cfg_obj_t *vconfig,
		   const cfg_obj_t *config, acl_type_t acltype,
		   cfg_aclconfctx_t *actx, dns_zone_t *zone,
		   void (*setzacl)(dns_zone_t *, dns_acl_t *),
		   void (*clearzacl)(dns_zone_t *))
{
	isc_result_t result;
	const cfg_obj_t *maps[5] = {NULL, NULL, NULL, NULL, NULL};
	const cfg_obj_t *aclobj = NULL;
	int i = 0;
	dns_acl_t **aclp = NULL, *acl = NULL;
	const char *aclname;
	dns_view_t *view;

	view = dns_zone_getview(zone);

	switch (acltype) {
	    case allow_notify:
		if (view != NULL)
			aclp = &view->notifyacl;
		aclname = "allow-notify";
		break;
	    case allow_query:
		if (view != NULL)
			aclp = &view->queryacl;
		aclname = "allow-query";
		break;
	    case allow_transfer:
		if (view != NULL)
			aclp = &view->transferacl;
		aclname = "allow-transfer";
		break;
	    case allow_update:
		if (view != NULL)
			aclp = &view->updateacl;
		aclname = "allow-update";
		break;
	    case allow_update_forwarding:
		if (view != NULL)
			aclp = &view->upfwdacl;
		aclname = "allow-update-forwarding";
		break;
	    default:
		INSIST(0);
		return (ISC_R_FAILURE);
	}

	/* First check to see if ACL is defined within the zone */
	if (zconfig != NULL) {
		maps[0] = cfg_tuple_get(zconfig, "options");
		ns_config_get(maps, aclname, &aclobj);
		if (aclobj != NULL) {
			aclp = NULL;
			goto parse_acl;
		}
	}

	/* Failing that, see if there's a default ACL already in the view */
	if (aclp != NULL && *aclp != NULL) {
		(*setzacl)(zone, *aclp);
		return (ISC_R_SUCCESS);
	}

	/* Check for default ACLs that haven't been parsed yet */
	if (vconfig != NULL) {
		const cfg_obj_t *options = cfg_tuple_get(vconfig, "options");
		if (options != NULL)
			maps[i++] = options;
	}
	if (config != NULL) {
		const cfg_obj_t *options = NULL;
		(void)cfg_map_get(config, "options", &options);
		if (options != NULL)
			maps[i++] = options;
	}
	maps[i++] = ns_g_defaults;
	maps[i] = NULL;

	result = ns_config_get(maps, aclname, &aclobj);
	if (aclobj == NULL) {
		(*clearzacl)(zone);
		return (ISC_R_SUCCESS);
	}

parse_acl:
	result = cfg_acl_fromconfig(aclobj, config, ns_g_lctx, actx,
				    dns_zone_getmctx(zone), 0, &acl);
	if (result != ISC_R_SUCCESS)
		return (result);
	(*setzacl)(zone, acl);

	/* Set the view default now */
	if (aclp != NULL)
		dns_acl_attach(acl, aclp);

	dns_acl_detach(&acl);
	return (ISC_R_SUCCESS);
}

/*%
 * Parse the zone update-policy statement.
 */
static isc_result_t
configure_zone_ssutable(const cfg_obj_t *zconfig, dns_zone_t *zone,
			const char *zname)
{
	const cfg_obj_t *updatepolicy = NULL;
	const cfg_listelt_t *element, *element2;
	dns_ssutable_t *table = NULL;
	isc_mem_t *mctx = dns_zone_getmctx(zone);
	isc_boolean_t autoddns = ISC_FALSE;
	isc_result_t result;

	(void)cfg_map_get(zconfig, "update-policy", &updatepolicy);

	if (updatepolicy == NULL) {
		dns_zone_setssutable(zone, NULL);
		return (ISC_R_SUCCESS);
	}

	if (cfg_obj_isstring(updatepolicy) &&
	    strcmp("local", cfg_obj_asstring(updatepolicy)) == 0) {
		autoddns = ISC_TRUE;
		updatepolicy = NULL;
	}

	result = dns_ssutable_create(mctx, &table);
	if (result != ISC_R_SUCCESS)
		return (result);

	for (element = cfg_list_first(updatepolicy);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const cfg_obj_t *stmt = cfg_listelt_value(element);
		const cfg_obj_t *mode = cfg_tuple_get(stmt, "mode");
		const cfg_obj_t *identity = cfg_tuple_get(stmt, "identity");
		const cfg_obj_t *matchtype = cfg_tuple_get(stmt, "matchtype");
		const cfg_obj_t *dname = cfg_tuple_get(stmt, "name");
		const cfg_obj_t *typelist = cfg_tuple_get(stmt, "types");
		const char *str;
		isc_boolean_t grant = ISC_FALSE;
		isc_boolean_t usezone = ISC_FALSE;
		unsigned int mtype = DNS_SSUMATCHTYPE_NAME;
		dns_fixedname_t fname, fident;
		isc_buffer_t b;
		dns_rdatatype_t *types;
		unsigned int i, n;

		str = cfg_obj_asstring(mode);
		if (strcasecmp(str, "grant") == 0)
			grant = ISC_TRUE;
		else if (strcasecmp(str, "deny") == 0)
			grant = ISC_FALSE;
		else
			INSIST(0);

		str = cfg_obj_asstring(matchtype);
		if (strcasecmp(str, "name") == 0)
			mtype = DNS_SSUMATCHTYPE_NAME;
		else if (strcasecmp(str, "subdomain") == 0)
			mtype = DNS_SSUMATCHTYPE_SUBDOMAIN;
		else if (strcasecmp(str, "wildcard") == 0)
			mtype = DNS_SSUMATCHTYPE_WILDCARD;
		else if (strcasecmp(str, "self") == 0)
			mtype = DNS_SSUMATCHTYPE_SELF;
		else if (strcasecmp(str, "selfsub") == 0)
			mtype = DNS_SSUMATCHTYPE_SELFSUB;
		else if (strcasecmp(str, "selfwild") == 0)
			mtype = DNS_SSUMATCHTYPE_SELFWILD;
		else if (strcasecmp(str, "ms-self") == 0)
			mtype = DNS_SSUMATCHTYPE_SELFMS;
		else if (strcasecmp(str, "krb5-self") == 0)
			mtype = DNS_SSUMATCHTYPE_SELFKRB5;
		else if (strcasecmp(str, "ms-subdomain") == 0)
			mtype = DNS_SSUMATCHTYPE_SUBDOMAINMS;
		else if (strcasecmp(str, "krb5-subdomain") == 0)
			mtype = DNS_SSUMATCHTYPE_SUBDOMAINKRB5;
		else if (strcasecmp(str, "tcp-self") == 0)
			mtype = DNS_SSUMATCHTYPE_TCPSELF;
		else if (strcasecmp(str, "6to4-self") == 0)
			mtype = DNS_SSUMATCHTYPE_6TO4SELF;
		else if (strcasecmp(str, "zonesub") == 0) {
			mtype = DNS_SSUMATCHTYPE_SUBDOMAIN;
			usezone = ISC_TRUE;
		} else if (strcasecmp(str, "external") == 0)
			mtype = DNS_SSUMATCHTYPE_EXTERNAL;
		else
			INSIST(0);

		dns_fixedname_init(&fident);
		str = cfg_obj_asstring(identity);
		isc_buffer_init(&b, str, strlen(str));
		isc_buffer_add(&b, strlen(str));
		result = dns_name_fromtext(dns_fixedname_name(&fident), &b,
					   dns_rootname, 0, NULL);
		if (result != ISC_R_SUCCESS) {
			cfg_obj_log(identity, ns_g_lctx, ISC_LOG_ERROR,
				    "'%s' is not a valid name", str);
			goto cleanup;
		}

		dns_fixedname_init(&fname);
		if (usezone) {
			result = dns_name_copy(dns_zone_getorigin(zone),
					       dns_fixedname_name(&fname),
					       NULL);
			if (result != ISC_R_SUCCESS) {
				cfg_obj_log(identity, ns_g_lctx, ISC_LOG_ERROR,
					    "error copying origin: %s",
					    isc_result_totext(result));
				goto cleanup;
			}
		} else {
			str = cfg_obj_asstring(dname);
			isc_buffer_init(&b, str, strlen(str));
			isc_buffer_add(&b, strlen(str));
			result = dns_name_fromtext(dns_fixedname_name(&fname),
						   &b, dns_rootname, 0, NULL);
			if (result != ISC_R_SUCCESS) {
				cfg_obj_log(identity, ns_g_lctx, ISC_LOG_ERROR,
					    "'%s' is not a valid name", str);
				goto cleanup;
			}
		}

		n = ns_config_listcount(typelist);
		if (n == 0)
			types = NULL;
		else {
			types = isc_mem_get(mctx, n * sizeof(dns_rdatatype_t));
			if (types == NULL) {
				result = ISC_R_NOMEMORY;
				goto cleanup;
			}
		}

		i = 0;
		for (element2 = cfg_list_first(typelist);
		     element2 != NULL;
		     element2 = cfg_list_next(element2))
		{
			const cfg_obj_t *typeobj;
			isc_textregion_t r;

			INSIST(i < n);

			typeobj = cfg_listelt_value(element2);
			str = cfg_obj_asstring(typeobj);
			DE_CONST(str, r.base);
			r.length = strlen(str);

			result = dns_rdatatype_fromtext(&types[i++], &r);
			if (result != ISC_R_SUCCESS) {
				cfg_obj_log(identity, ns_g_lctx, ISC_LOG_ERROR,
					    "'%s' is not a valid type", str);
				isc_mem_put(mctx, types,
					    n * sizeof(dns_rdatatype_t));
				goto cleanup;
			}
		}
		INSIST(i == n);

		result = dns_ssutable_addrule(table, grant,
					      dns_fixedname_name(&fident),
					      mtype,
					      dns_fixedname_name(&fname),
					      n, types);
		if (types != NULL)
			isc_mem_put(mctx, types, n * sizeof(dns_rdatatype_t));
		if (result != ISC_R_SUCCESS) {
			goto cleanup;
		}
	}

	/*
	 * If "update-policy local;" and a session key exists,
	 * then use the default policy, which is equivalent to:
	 * update-policy { grant <session-keyname> zonesub any; };
	 */
	if (autoddns) {
		dns_rdatatype_t any = dns_rdatatype_any;

		if (ns_g_server->session_keyname == NULL) {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
				      "failed to enable auto DDNS policy "
				      "for zone %s: session key not found",
				      zname);
			result = ISC_R_NOTFOUND;
			goto cleanup;
		}

		result = dns_ssutable_addrule(table, ISC_TRUE,
					      ns_g_server->session_keyname,
					      DNS_SSUMATCHTYPE_SUBDOMAIN,
					      dns_zone_getorigin(zone),
					      1, &any);

		if (result != ISC_R_SUCCESS)
			goto cleanup;
	}

	result = ISC_R_SUCCESS;
	dns_zone_setssutable(zone, table);

 cleanup:
	dns_ssutable_detach(&table);
	return (result);
}

/*
 * This is the TTL used for internally generated RRsets for static-stub zones.
 * The value doesn't matter because the mapping is static, but needs to be
 * defined for the sake of implementation.
 */
#define STATICSTUB_SERVER_TTL 86400

/*%
 * Configure an apex NS with glues for a static-stub zone.
 * For example, for the zone named "example.com", the following RRs will be
 * added to the zone DB:
 * example.com. NS example.com.
 * example.com. A 192.0.2.1
 * example.com. AAAA 2001:db8::1
 */
static isc_result_t
configure_staticstub_serveraddrs(const cfg_obj_t *zconfig, dns_zone_t *zone,
				 dns_rdatalist_t *rdatalist_ns,
				 dns_rdatalist_t *rdatalist_a,
				 dns_rdatalist_t *rdatalist_aaaa)
{
	const cfg_listelt_t *element;
	isc_mem_t *mctx = dns_zone_getmctx(zone);
	isc_region_t region, sregion;
	dns_rdata_t *rdata;
	isc_result_t result = ISC_R_SUCCESS;

	for (element = cfg_list_first(zconfig);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const isc_sockaddr_t* sa;
		isc_netaddr_t na;
		const cfg_obj_t *address = cfg_listelt_value(element);
		dns_rdatalist_t *rdatalist;

		sa = cfg_obj_assockaddr(address);
		if (isc_sockaddr_getport(sa) != 0) {
			cfg_obj_log(zconfig, ns_g_lctx, ISC_LOG_ERROR,
				    "port is not configurable for "
				    "static stub server-addresses");
			return (ISC_R_FAILURE);
		}
		isc_netaddr_fromsockaddr(&na, sa);
		if (isc_netaddr_getzone(&na) != 0) {
			cfg_obj_log(zconfig, ns_g_lctx, ISC_LOG_ERROR,
					    "scoped address is not allowed "
					    "for static stub "
					    "server-addresses");
			return (ISC_R_FAILURE);
		}

		switch (na.family) {
		case AF_INET:
			region.length = sizeof(na.type.in);
			rdatalist = rdatalist_a;
			break;
		default:
			INSIST(na.family == AF_INET6);
			region.length = sizeof(na.type.in6);
			rdatalist = rdatalist_aaaa;
			break;
		}

		rdata = isc_mem_get(mctx, sizeof(*rdata) + region.length);
		if (rdata == NULL)
			return (ISC_R_NOMEMORY);
		region.base = (unsigned char *)(rdata + 1);
		memcpy(region.base, &na.type, region.length);
		dns_rdata_init(rdata);
		dns_rdata_fromregion(rdata, dns_zone_getclass(zone),
				     rdatalist->type, &region);
		ISC_LIST_APPEND(rdatalist->rdata, rdata, link);
	}

	/*
	 * If no address is specified (unlikely in this context, but possible),
	 * there's nothing to do anymore.
	 */
	if (ISC_LIST_EMPTY(rdatalist_a->rdata) &&
	    ISC_LIST_EMPTY(rdatalist_aaaa->rdata)) {
		return (ISC_R_SUCCESS);
	}

	/* Add to the list an apex NS with the ns name being the origin name */
	dns_name_toregion(dns_zone_getorigin(zone), &sregion);
	rdata = isc_mem_get(mctx, sizeof(*rdata) + sregion.length);
	if (rdata == NULL) {
		/*
		 * Already allocated data will be freed in the caller, so
		 * we can simply return here.
		 */
		return (ISC_R_NOMEMORY);
	}
	region.length = sregion.length;
	region.base = (unsigned char *)(rdata + 1);
	memcpy(region.base, sregion.base, region.length);
	dns_rdata_init(rdata);
	dns_rdata_fromregion(rdata, dns_zone_getclass(zone),
			     dns_rdatatype_ns, &region);
	ISC_LIST_APPEND(rdatalist_ns->rdata, rdata, link);

	return (result);
}

/*%
 * Configure an apex NS with an out-of-zone NS names for a static-stub zone.
 * For example, for the zone named "example.com", something like the following
 * RRs will be added to the zone DB:
 * example.com. NS ns.example.net.
 */
static isc_result_t
configure_staticstub_servernames(const cfg_obj_t *zconfig, dns_zone_t *zone,
				 dns_rdatalist_t *rdatalist, const char *zname)
{
	const cfg_listelt_t *element;
	isc_mem_t *mctx = dns_zone_getmctx(zone);
	dns_rdata_t *rdata;
	isc_region_t sregion, region;
	isc_result_t result = ISC_R_SUCCESS;

	for (element = cfg_list_first(zconfig);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const cfg_obj_t *obj;
		const char *str;
		dns_fixedname_t fixed_name;
		dns_name_t *nsname;
		isc_buffer_t b;

		obj = cfg_listelt_value(element);
		str = cfg_obj_asstring(obj);

		dns_fixedname_init(&fixed_name);
		nsname = dns_fixedname_name(&fixed_name);

		isc_buffer_init(&b, str, strlen(str));
		isc_buffer_add(&b, strlen(str));
		result = dns_name_fromtext(nsname, &b, dns_rootname, 0, NULL);
		if (result != ISC_R_SUCCESS) {
			cfg_obj_log(zconfig, ns_g_lctx, ISC_LOG_ERROR,
					    "server-name '%s' is not a valid "
					    "name", str);
			return (result);
		}
		if (dns_name_issubdomain(nsname, dns_zone_getorigin(zone))) {
			cfg_obj_log(zconfig, ns_g_lctx, ISC_LOG_ERROR,
				    "server-name '%s' must not be a "
				    "subdomain of zone name '%s'",
				    str, zname);
			return (ISC_R_FAILURE);
		}

		dns_name_toregion(nsname, &sregion);
		rdata = isc_mem_get(mctx, sizeof(*rdata) + sregion.length);
		if (rdata == NULL)
			return (ISC_R_NOMEMORY);
		region.length = sregion.length;
		region.base = (unsigned char *)(rdata + 1);
		memcpy(region.base, sregion.base, region.length);
		dns_rdata_init(rdata);
		dns_rdata_fromregion(rdata, dns_zone_getclass(zone),
				     dns_rdatatype_ns, &region);
		ISC_LIST_APPEND(rdatalist->rdata, rdata, link);
	}

	return (result);
}

/*%
 * Configure static-stub zone.
 */
static isc_result_t
configure_staticstub(const cfg_obj_t *zconfig, dns_zone_t *zone,
		     const char *zname, const char *dbtype)
{
	int i = 0;
	const cfg_obj_t *obj;
	isc_mem_t *mctx = dns_zone_getmctx(zone);
	dns_db_t *db = NULL;
	dns_dbversion_t *dbversion = NULL;
	dns_dbnode_t *apexnode = NULL;
	dns_name_t apexname;
	isc_result_t result;
	dns_rdataset_t rdataset;
	dns_rdatalist_t rdatalist_ns, rdatalist_a, rdatalist_aaaa;
	dns_rdatalist_t* rdatalists[] = {
		&rdatalist_ns, &rdatalist_a, &rdatalist_aaaa, NULL
	};
	dns_rdata_t *rdata;
	isc_region_t region;

	/* Create the DB beforehand */
	RETERR(dns_db_create(mctx, dbtype, dns_zone_getorigin(zone),
			     dns_dbtype_stub, dns_zone_getclass(zone),
			     0, NULL, &db));
	dns_zone_setdb(zone, db);

	dns_rdatalist_init(&rdatalist_ns);
	rdatalist_ns.rdclass = dns_zone_getclass(zone);
	rdatalist_ns.type = dns_rdatatype_ns;
	rdatalist_ns.ttl = STATICSTUB_SERVER_TTL;

	dns_rdatalist_init(&rdatalist_a);
	rdatalist_a.rdclass = dns_zone_getclass(zone);
	rdatalist_a.type = dns_rdatatype_a;
	rdatalist_a.ttl = STATICSTUB_SERVER_TTL;

	dns_rdatalist_init(&rdatalist_aaaa);
	rdatalist_aaaa.rdclass = dns_zone_getclass(zone);
	rdatalist_aaaa.type = dns_rdatatype_aaaa;
	rdatalist_aaaa.ttl = STATICSTUB_SERVER_TTL;

	/* Prepare zone RRs from the configuration */
	obj = NULL;
	result = cfg_map_get(zconfig, "server-addresses", &obj);
	if (obj != NULL) {
		result = configure_staticstub_serveraddrs(obj, zone,
							  &rdatalist_ns,
							  &rdatalist_a,
							  &rdatalist_aaaa);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
	}

	obj = NULL;
	result = cfg_map_get(zconfig, "server-names", &obj);
	if (obj != NULL) {
		result = configure_staticstub_servernames(obj, zone,
							  &rdatalist_ns,
							  zname);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
	}

	/*
	 * Sanity check: there should be at least one NS RR at the zone apex
	 * to trigger delegation.
	 */
	if (ISC_LIST_EMPTY(rdatalist_ns.rdata)) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "No NS record is configured for a "
			      "static-stub zone '%s'", zname);
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	/*
	 * Now add NS and glue A/AAAA RRsets to the zone DB.
	 * First open a new version for the add operation and get a pointer
	 * to the apex node (all RRs are of the apex name).
	 */
	result = dns_db_newversion(db, &dbversion);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
	dns_name_init(&apexname, NULL);
	dns_name_clone(dns_zone_getorigin(zone), &apexname);
	result = dns_db_findnode(db, &apexname, ISC_FALSE, &apexnode);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	/* Add NS RRset */
	dns_rdataset_init(&rdataset);
	RUNTIME_CHECK(dns_rdatalist_tordataset(&rdatalist_ns, &rdataset)
		      == ISC_R_SUCCESS);
	result = dns_db_addrdataset(db, apexnode, dbversion, 0, &rdataset,
				    0, NULL);
	dns_rdataset_disassociate(&rdataset);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	/* Add glue A RRset, if any */
	if (!ISC_LIST_EMPTY(rdatalist_a.rdata)) {
		RUNTIME_CHECK(dns_rdatalist_tordataset(&rdatalist_a, &rdataset)
			      == ISC_R_SUCCESS);
		result = dns_db_addrdataset(db, apexnode, dbversion, 0,
					    &rdataset, 0, NULL);
		dns_rdataset_disassociate(&rdataset);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
	}

	/* Add glue AAAA RRset, if any */
	if (!ISC_LIST_EMPTY(rdatalist_aaaa.rdata)) {
		RUNTIME_CHECK(dns_rdatalist_tordataset(&rdatalist_aaaa,
						       &rdataset)
			      == ISC_R_SUCCESS);
		result = dns_db_addrdataset(db, apexnode, dbversion, 0,
					    &rdataset, 0, NULL);
		dns_rdataset_disassociate(&rdataset);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
	}

	result = ISC_R_SUCCESS;

  cleanup:
	if (apexnode != NULL)
		dns_db_detachnode(db, &apexnode);
	if (dbversion != NULL)
		dns_db_closeversion(db, &dbversion, ISC_TRUE);
	if (db != NULL)
		dns_db_detach(&db);
	for (i = 0; rdatalists[i] != NULL; i++) {
		while ((rdata = ISC_LIST_HEAD(rdatalists[i]->rdata)) != NULL) {
			ISC_LIST_UNLINK(rdatalists[i]->rdata, rdata, link);
			dns_rdata_toregion(rdata, &region);
			isc_mem_put(mctx, rdata,
				    sizeof(*rdata) + region.length);
		}
	}

	return (result);
}

/*%
 * Convert a config file zone type into a server zone type.
 */
static inline dns_zonetype_t
zonetype_fromconfig(const cfg_obj_t *map) {
	const cfg_obj_t *obj = NULL;
	isc_result_t result;

	result = cfg_map_get(map, "type", &obj);
	INSIST(result == ISC_R_SUCCESS);
	return (ns_config_getzonetype(obj));
}

/*%
 * Helper function for strtoargv().  Pardon the gratuitous recursion.
 */
static isc_result_t
strtoargvsub(isc_mem_t *mctx, char *s, unsigned int *argcp,
	     char ***argvp, unsigned int n)
{
	isc_result_t result;

	/* Discard leading whitespace. */
	while (*s == ' ' || *s == '\t')
		s++;

	if (*s == '\0') {
		/* We have reached the end of the string. */
		*argcp = n;
		*argvp = isc_mem_get(mctx, n * sizeof(char *));
		if (*argvp == NULL)
			return (ISC_R_NOMEMORY);
	} else {
		char *p = s;
		while (*p != ' ' && *p != '\t' && *p != '\0')
			p++;
		if (*p != '\0')
			*p++ = '\0';

		result = strtoargvsub(mctx, p, argcp, argvp, n + 1);
		if (result != ISC_R_SUCCESS)
			return (result);
		(*argvp)[n] = s;
	}
	return (ISC_R_SUCCESS);
}

/*%
 * Tokenize the string "s" into whitespace-separated words,
 * return the number of words in '*argcp' and an array
 * of pointers to the words in '*argvp'.  The caller
 * must free the array using isc_mem_put().  The string
 * is modified in-place.
 */
static isc_result_t
strtoargv(isc_mem_t *mctx, char *s, unsigned int *argcp, char ***argvp) {
	return (strtoargvsub(mctx, s, argcp, argvp, 0));
}

static void
checknames(dns_zonetype_t ztype, const cfg_obj_t **maps,
	   const cfg_obj_t **objp)
{
	const char *zone = NULL;
	isc_result_t result;

	switch (ztype) {
	case dns_zone_slave: zone = "slave"; break;
	case dns_zone_master: zone = "master"; break;
	default:
		INSIST(0);
	}
	result = ns_checknames_get(maps, zone, objp);
	INSIST(result == ISC_R_SUCCESS);
}

isc_result_t
ns_zone_configure(const cfg_obj_t *config, const cfg_obj_t *vconfig,
		  const cfg_obj_t *zconfig, cfg_aclconfctx_t *ac,
		  dns_zone_t *zone)
{
	isc_result_t result;
	const char *zname;
	dns_rdataclass_t zclass;
	dns_rdataclass_t vclass;
	const cfg_obj_t *maps[5];
	const cfg_obj_t *zoptions = NULL;
	const cfg_obj_t *options = NULL;
	const cfg_obj_t *obj;
	const char *filename = NULL;
	dns_notifytype_t notifytype = dns_notifytype_yes;
	isc_sockaddr_t *addrs;
	dns_name_t **keynames;
	isc_uint32_t count;
	char *cpval;
	unsigned int dbargc;
	char **dbargv;
	static char default_dbtype[] = "rbt";
	isc_mem_t *mctx = dns_zone_getmctx(zone);
	dns_dialuptype_t dialup = dns_dialuptype_no;
	dns_zonetype_t ztype;
	int i;
	isc_int32_t journal_size;
	isc_boolean_t multi;
	isc_boolean_t alt;
	dns_view_t *view;
	isc_boolean_t check = ISC_FALSE, fail = ISC_FALSE;
	isc_boolean_t warn = ISC_FALSE, ignore = ISC_FALSE;
	isc_boolean_t ixfrdiff;
	dns_masterformat_t masterformat;
	isc_stats_t *zoneqrystats;
	isc_boolean_t zonestats_on;
	int seconds;

	i = 0;
	if (zconfig != NULL) {
		zoptions = cfg_tuple_get(zconfig, "options");
		maps[i++] = zoptions;
	}
	if (vconfig != NULL)
		maps[i++] = cfg_tuple_get(vconfig, "options");
	if (config != NULL) {
		(void)cfg_map_get(config, "options", &options);
		if (options != NULL)
			maps[i++] = options;
	}
	maps[i++] = ns_g_defaults;
	maps[i++] = NULL;

	if (vconfig != NULL)
		RETERR(ns_config_getclass(cfg_tuple_get(vconfig, "class"),
					  dns_rdataclass_in, &vclass));
	else
		vclass = dns_rdataclass_in;

	/*
	 * Configure values common to all zone types.
	 */

	zname = cfg_obj_asstring(cfg_tuple_get(zconfig, "name"));

	RETERR(ns_config_getclass(cfg_tuple_get(zconfig, "class"),
				  vclass, &zclass));
	dns_zone_setclass(zone, zclass);

	ztype = zonetype_fromconfig(zoptions);
	dns_zone_settype(zone, ztype);

	obj = NULL;
	result = cfg_map_get(zoptions, "database", &obj);
	if (result == ISC_R_SUCCESS)
		cpval = isc_mem_strdup(mctx, cfg_obj_asstring(obj));
	else
		cpval = default_dbtype;

	if (cpval == NULL)
		return(ISC_R_NOMEMORY);

	result = strtoargv(mctx, cpval, &dbargc, &dbargv);
	if (result != ISC_R_SUCCESS && cpval != default_dbtype) {
		isc_mem_free(mctx, cpval);
		return (result);
	}

	/*
	 * ANSI C is strange here.  There is no logical reason why (char **)
	 * cannot be promoted automatically to (const char * const *) by the
	 * compiler w/o generating a warning.
	 */
	result = dns_zone_setdbtype(zone, dbargc, (const char * const *)dbargv);
	isc_mem_put(mctx, dbargv, dbargc * sizeof(*dbargv));
	if (cpval != default_dbtype)
		isc_mem_free(mctx, cpval);
	if (result != ISC_R_SUCCESS)
		return (result);

	obj = NULL;
	result = cfg_map_get(zoptions, "file", &obj);
	if (result == ISC_R_SUCCESS)
		filename = cfg_obj_asstring(obj);

	/*
	 * Unless we're using some alternative database, a master zone
	 * will be needing a master file.
	 */
	if (ztype == dns_zone_master && cpval == default_dbtype &&
	    filename == NULL) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "zone '%s': 'file' not specified",
			      zname);
		return (ISC_R_FAILURE);
	}

	masterformat = dns_masterformat_text;
	obj = NULL;
	result= ns_config_get(maps, "masterfile-format", &obj);
	if (result == ISC_R_SUCCESS) {
		const char *masterformatstr = cfg_obj_asstring(obj);

		if (strcasecmp(masterformatstr, "text") == 0)
			masterformat = dns_masterformat_text;
		else if (strcasecmp(masterformatstr, "raw") == 0)
			masterformat = dns_masterformat_raw;
		else
			INSIST(0);
	}
	RETERR(dns_zone_setfile2(zone, filename, masterformat));

	obj = NULL;
	result = cfg_map_get(zoptions, "journal", &obj);
	if (result == ISC_R_SUCCESS)
		RETERR(dns_zone_setjournal(zone, cfg_obj_asstring(obj)));

	if (ztype == dns_zone_slave)
		RETERR(configure_zone_acl(zconfig, vconfig, config,
					  allow_notify, ac, zone,
					  dns_zone_setnotifyacl,
					  dns_zone_clearnotifyacl));
	/*
	 * XXXAG This probably does not make sense for stubs.
	 */
	RETERR(configure_zone_acl(zconfig, vconfig, config,
				  allow_query, ac, zone,
				  dns_zone_setqueryacl,
				  dns_zone_clearqueryacl));

	obj = NULL;
	result = ns_config_get(maps, "dialup", &obj);
	INSIST(result == ISC_R_SUCCESS);
	if (cfg_obj_isboolean(obj)) {
		if (cfg_obj_asboolean(obj))
			dialup = dns_dialuptype_yes;
		else
			dialup = dns_dialuptype_no;
	} else {
		const char *dialupstr = cfg_obj_asstring(obj);
		if (strcasecmp(dialupstr, "notify") == 0)
			dialup = dns_dialuptype_notify;
		else if (strcasecmp(dialupstr, "notify-passive") == 0)
			dialup = dns_dialuptype_notifypassive;
		else if (strcasecmp(dialupstr, "refresh") == 0)
			dialup = dns_dialuptype_refresh;
		else if (strcasecmp(dialupstr, "passive") == 0)
			dialup = dns_dialuptype_passive;
		else
			INSIST(0);
	}
	dns_zone_setdialup(zone, dialup);

	obj = NULL;
	result = ns_config_get(maps, "zone-statistics", &obj);
	INSIST(result == ISC_R_SUCCESS);
	zonestats_on = cfg_obj_asboolean(obj);
	zoneqrystats = NULL;
	if (zonestats_on) {
		RETERR(isc_stats_create(mctx, &zoneqrystats,
					dns_nsstatscounter_max));
	}
	dns_zone_setrequeststats(zone, zoneqrystats);
	if (zoneqrystats != NULL)
		isc_stats_detach(&zoneqrystats);

	/*
	 * Configure master functionality.  This applies
	 * to primary masters (type "master") and slaves
	 * acting as masters (type "slave"), but not to stubs.
	 */
	if (ztype != dns_zone_stub && ztype != dns_zone_staticstub) {
		obj = NULL;
		result = ns_config_get(maps, "notify", &obj);
		INSIST(result == ISC_R_SUCCESS);
		if (cfg_obj_isboolean(obj)) {
			if (cfg_obj_asboolean(obj))
				notifytype = dns_notifytype_yes;
			else
				notifytype = dns_notifytype_no;
		} else {
			const char *notifystr = cfg_obj_asstring(obj);
			if (strcasecmp(notifystr, "explicit") == 0)
				notifytype = dns_notifytype_explicit;
			else if (strcasecmp(notifystr, "master-only") == 0)
				notifytype = dns_notifytype_masteronly;
			else
				INSIST(0);
		}
		dns_zone_setnotifytype(zone, notifytype);

		obj = NULL;
		result = ns_config_get(maps, "also-notify", &obj);
		if (result == ISC_R_SUCCESS) {
			isc_sockaddr_t *addrs = NULL;
			isc_uint32_t addrcount;
			result = ns_config_getiplist(config, obj, 0, mctx,
						     &addrs, &addrcount);
			if (result != ISC_R_SUCCESS)
				return (result);
			result = dns_zone_setalsonotify(zone, addrs,
							addrcount);
			ns_config_putiplist(mctx, &addrs, addrcount);
			if (result != ISC_R_SUCCESS)
				return (result);
		} else
			RETERR(dns_zone_setalsonotify(zone, NULL, 0));

		obj = NULL;
		result = ns_config_get(maps, "notify-source", &obj);
		INSIST(result == ISC_R_SUCCESS);
		RETERR(dns_zone_setnotifysrc4(zone, cfg_obj_assockaddr(obj)));
		ns_add_reserved_dispatch(ns_g_server, cfg_obj_assockaddr(obj));

		obj = NULL;
		result = ns_config_get(maps, "notify-source-v6", &obj);
		INSIST(result == ISC_R_SUCCESS);
		RETERR(dns_zone_setnotifysrc6(zone, cfg_obj_assockaddr(obj)));
		ns_add_reserved_dispatch(ns_g_server, cfg_obj_assockaddr(obj));

		obj = NULL;
		result = ns_config_get(maps, "notify-to-soa", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setoption(zone, DNS_ZONEOPT_NOTIFYTOSOA,
				   cfg_obj_asboolean(obj));

		dns_zone_setisself(zone, ns_client_isself, NULL);

		RETERR(configure_zone_acl(zconfig, vconfig, config,
					  allow_transfer, ac, zone,
					  dns_zone_setxfracl,
					  dns_zone_clearxfracl));

		obj = NULL;
		result = ns_config_get(maps, "max-transfer-time-out", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setmaxxfrout(zone, cfg_obj_asuint32(obj) * 60);

		obj = NULL;
		result = ns_config_get(maps, "max-transfer-idle-out", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setidleout(zone, cfg_obj_asuint32(obj) * 60);

		obj = NULL;
		result =  ns_config_get(maps, "max-journal-size", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setjournalsize(zone, -1);
		if (cfg_obj_isstring(obj)) {
			const char *str = cfg_obj_asstring(obj);
			INSIST(strcasecmp(str, "unlimited") == 0);
			journal_size = ISC_UINT32_MAX / 2;
		} else {
			isc_resourcevalue_t value;
			value = cfg_obj_asuint64(obj);
			if (value > ISC_UINT32_MAX / 2) {
				cfg_obj_log(obj, ns_g_lctx,
					    ISC_LOG_ERROR,
					    "'max-journal-size "
					    "%" ISC_PRINT_QUADFORMAT "d' "
					    "is too large",
					    value);
				RETERR(ISC_R_RANGE);
			}
			journal_size = (isc_uint32_t)value;
		}
		dns_zone_setjournalsize(zone, journal_size);

		obj = NULL;
		result = ns_config_get(maps, "ixfr-from-differences", &obj);
		INSIST(result == ISC_R_SUCCESS);
		if (cfg_obj_isboolean(obj))
			ixfrdiff = cfg_obj_asboolean(obj);
		else if (strcasecmp(cfg_obj_asstring(obj), "master") &&
			 ztype == dns_zone_master)
			ixfrdiff = ISC_TRUE;
		else if (strcasecmp(cfg_obj_asstring(obj), "slave") &&
			ztype == dns_zone_slave)
			ixfrdiff = ISC_TRUE;
		else
			ixfrdiff = ISC_FALSE;
		dns_zone_setoption(zone, DNS_ZONEOPT_IXFRFROMDIFFS, ixfrdiff);

		checknames(ztype, maps, &obj);
		INSIST(obj != NULL);
		if (strcasecmp(cfg_obj_asstring(obj), "warn") == 0) {
			fail = ISC_FALSE;
			check = ISC_TRUE;
		} else if (strcasecmp(cfg_obj_asstring(obj), "fail") == 0) {
			fail = check = ISC_TRUE;
		} else if (strcasecmp(cfg_obj_asstring(obj), "ignore") == 0) {
			fail = check = ISC_FALSE;
		} else
			INSIST(0);
		dns_zone_setoption(zone, DNS_ZONEOPT_CHECKNAMES, check);
		dns_zone_setoption(zone, DNS_ZONEOPT_CHECKNAMESFAIL, fail);

		obj = NULL;
		result = ns_config_get(maps, "notify-delay", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setnotifydelay(zone, cfg_obj_asuint32(obj));

		obj = NULL;
		result = ns_config_get(maps, "check-sibling", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setoption(zone, DNS_ZONEOPT_CHECKSIBLING,
				   cfg_obj_asboolean(obj));

		obj = NULL;
		result = ns_config_get(maps, "zero-no-soa-ttl", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setzeronosoattl(zone, cfg_obj_asboolean(obj));

		obj = NULL;
		result = ns_config_get(maps, "nsec3-test-zone", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setoption(zone, DNS_ZONEOPT_NSEC3TESTZONE,
				   cfg_obj_asboolean(obj));
	}

	/*
	 * Configure update-related options.  These apply to
	 * primary masters only.
	 */
	if (ztype == dns_zone_master) {
		dns_acl_t *updateacl;

		RETERR(configure_zone_acl(zconfig, vconfig, config,
					  allow_update, ac, zone,
					  dns_zone_setupdateacl,
					  dns_zone_clearupdateacl));

		updateacl = dns_zone_getupdateacl(zone);
		if (updateacl != NULL  && dns_acl_isinsecure(updateacl))
			isc_log_write(ns_g_lctx, DNS_LOGCATEGORY_SECURITY,
				      NS_LOGMODULE_SERVER, ISC_LOG_WARNING,
				      "zone '%s' allows updates by IP "
				      "address, which is insecure",
				      zname);

		RETERR(configure_zone_ssutable(zoptions, zone, zname));

		obj = NULL;
		result = ns_config_get(maps, "sig-validity-interval", &obj);
		INSIST(result == ISC_R_SUCCESS);
		{
			const cfg_obj_t *validity, *resign;

			validity = cfg_tuple_get(obj, "validity");
			seconds = cfg_obj_asuint32(validity) * 86400;
			dns_zone_setsigvalidityinterval(zone, seconds);

			resign = cfg_tuple_get(obj, "re-sign");
			if (cfg_obj_isvoid(resign)) {
				seconds /= 4;
			} else {
				if (seconds > 7 * 86400)
					seconds = cfg_obj_asuint32(resign) *
							86400;
				else
					seconds = cfg_obj_asuint32(resign) *
							3600;
			}
			dns_zone_setsigresigninginterval(zone, seconds);
		}

		obj = NULL;
		result = ns_config_get(maps, "key-directory", &obj);
		if (result == ISC_R_SUCCESS) {
			filename = cfg_obj_asstring(obj);
			RETERR(dns_zone_setkeydirectory(zone, filename));
		}

		obj = NULL;
		result = ns_config_get(maps, "sig-signing-signatures", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setsignatures(zone, cfg_obj_asuint32(obj));

		obj = NULL;
		result = ns_config_get(maps, "sig-signing-nodes", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setnodes(zone, cfg_obj_asuint32(obj));

		obj = NULL;
		result = ns_config_get(maps, "sig-signing-type", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setprivatetype(zone, cfg_obj_asuint32(obj));

		obj = NULL;
		result = ns_config_get(maps, "update-check-ksk", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setoption(zone, DNS_ZONEOPT_UPDATECHECKKSK,
				   cfg_obj_asboolean(obj));

		obj = NULL;
		result = ns_config_get(maps, "dnssec-dnskey-kskonly", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setoption(zone, DNS_ZONEOPT_DNSKEYKSKONLY,
				   cfg_obj_asboolean(obj));
	} else if (ztype == dns_zone_slave) {
		RETERR(configure_zone_acl(zconfig, vconfig, config,
					  allow_update_forwarding, ac, zone,
					  dns_zone_setforwardacl,
					  dns_zone_clearforwardacl));
	}

	/*%
	 * Primary master functionality.
	 */
	if (ztype == dns_zone_master) {
		isc_boolean_t allow = ISC_FALSE, maint = ISC_FALSE;
		isc_boolean_t create = ISC_FALSE;

		obj = NULL;
		result = ns_config_get(maps, "check-wildcard", &obj);
		if (result == ISC_R_SUCCESS)
			check = cfg_obj_asboolean(obj);
		else
			check = ISC_FALSE;
		dns_zone_setoption(zone, DNS_ZONEOPT_CHECKWILDCARD, check);

		obj = NULL;
		result = ns_config_get(maps, "check-dup-records", &obj);
		INSIST(obj != NULL);
		if (strcasecmp(cfg_obj_asstring(obj), "warn") == 0) {
			fail = ISC_FALSE;
			check = ISC_TRUE;
		} else if (strcasecmp(cfg_obj_asstring(obj), "fail") == 0) {
			fail = check = ISC_TRUE;
		} else if (strcasecmp(cfg_obj_asstring(obj), "ignore") == 0) {
			fail = check = ISC_FALSE;
		} else
			INSIST(0);
		dns_zone_setoption(zone, DNS_ZONEOPT_CHECKDUPRR, check);
		dns_zone_setoption(zone, DNS_ZONEOPT_CHECKDUPRRFAIL, fail);

		obj = NULL;
		result = ns_config_get(maps, "check-mx", &obj);
		INSIST(obj != NULL);
		if (strcasecmp(cfg_obj_asstring(obj), "warn") == 0) {
			fail = ISC_FALSE;
			check = ISC_TRUE;
		} else if (strcasecmp(cfg_obj_asstring(obj), "fail") == 0) {
			fail = check = ISC_TRUE;
		} else if (strcasecmp(cfg_obj_asstring(obj), "ignore") == 0) {
			fail = check = ISC_FALSE;
		} else
			INSIST(0);
		dns_zone_setoption(zone, DNS_ZONEOPT_CHECKMX, check);
		dns_zone_setoption(zone, DNS_ZONEOPT_CHECKMXFAIL, fail);

		obj = NULL;
		result = ns_config_get(maps, "check-integrity", &obj);
		INSIST(obj != NULL);
		dns_zone_setoption(zone, DNS_ZONEOPT_CHECKINTEGRITY,
				   cfg_obj_asboolean(obj));

		obj = NULL;
		result = ns_config_get(maps, "check-mx-cname", &obj);
		INSIST(obj != NULL);
		if (strcasecmp(cfg_obj_asstring(obj), "warn") == 0) {
			warn = ISC_TRUE;
			ignore = ISC_FALSE;
		} else if (strcasecmp(cfg_obj_asstring(obj), "fail") == 0) {
			warn = ignore = ISC_FALSE;
		} else if (strcasecmp(cfg_obj_asstring(obj), "ignore") == 0) {
			warn = ignore = ISC_TRUE;
		} else
			INSIST(0);
		dns_zone_setoption(zone, DNS_ZONEOPT_WARNMXCNAME, warn);
		dns_zone_setoption(zone, DNS_ZONEOPT_IGNOREMXCNAME, ignore);

		obj = NULL;
		result = ns_config_get(maps, "check-srv-cname", &obj);
		INSIST(obj != NULL);
		if (strcasecmp(cfg_obj_asstring(obj), "warn") == 0) {
			warn = ISC_TRUE;
			ignore = ISC_FALSE;
		} else if (strcasecmp(cfg_obj_asstring(obj), "fail") == 0) {
			warn = ignore = ISC_FALSE;
		} else if (strcasecmp(cfg_obj_asstring(obj), "ignore") == 0) {
			warn = ignore = ISC_TRUE;
		} else
			INSIST(0);
		dns_zone_setoption(zone, DNS_ZONEOPT_WARNSRVCNAME, warn);
		dns_zone_setoption(zone, DNS_ZONEOPT_IGNORESRVCNAME, ignore);

		obj = NULL;
		result = ns_config_get(maps, "dnssec-secure-to-insecure", &obj);
		INSIST(obj != NULL);
		dns_zone_setoption(zone, DNS_ZONEOPT_SECURETOINSECURE,
				   cfg_obj_asboolean(obj));

		obj = NULL;
		result = cfg_map_get(zoptions, "auto-dnssec", &obj);
		if (result == ISC_R_SUCCESS) {
			const char *arg = cfg_obj_asstring(obj);
			if (strcasecmp(arg, "allow") == 0)
				allow = ISC_TRUE;
			else if (strcasecmp(arg, "maintain") == 0)
				allow = maint = ISC_TRUE;
			else if (strcasecmp(arg, "create") == 0)
				allow = maint = create = ISC_TRUE;
			else if (strcasecmp(arg, "off") == 0)
				;
			else
				INSIST(0);
			dns_zone_setkeyopt(zone, DNS_ZONEKEY_ALLOW, allow);
			dns_zone_setkeyopt(zone, DNS_ZONEKEY_MAINTAIN, maint);
			dns_zone_setkeyopt(zone, DNS_ZONEKEY_CREATE, create);
		}
	}

	/*
	 * Configure slave functionality.
	 */
	switch (ztype) {
	case dns_zone_slave:
	case dns_zone_stub:
		count = 0;
		obj = NULL;
		result = cfg_map_get(zoptions, "masters", &obj);
		if (obj != NULL) {
			addrs = NULL;
			keynames = NULL;
			RETERR(ns_config_getipandkeylist(config, obj, mctx,
							 &addrs, &keynames,
							 &count));
			result = dns_zone_setmasterswithkeys(zone, addrs,
							     keynames, count);
			ns_config_putipandkeylist(mctx, &addrs, &keynames,
						  count);
		} else
			result = dns_zone_setmasters(zone, NULL, 0);
		RETERR(result);

		multi = ISC_FALSE;
		if (count > 1) {
			obj = NULL;
			result = ns_config_get(maps, "multi-master", &obj);
			INSIST(result == ISC_R_SUCCESS);
			multi = cfg_obj_asboolean(obj);
		}
		dns_zone_setoption(zone, DNS_ZONEOPT_MULTIMASTER, multi);

		obj = NULL;
		result = ns_config_get(maps, "max-transfer-time-in", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setmaxxfrin(zone, cfg_obj_asuint32(obj) * 60);

		obj = NULL;
		result = ns_config_get(maps, "max-transfer-idle-in", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setidlein(zone, cfg_obj_asuint32(obj) * 60);

		obj = NULL;
		result = ns_config_get(maps, "max-refresh-time", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setmaxrefreshtime(zone, cfg_obj_asuint32(obj));

		obj = NULL;
		result = ns_config_get(maps, "min-refresh-time", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setminrefreshtime(zone, cfg_obj_asuint32(obj));

		obj = NULL;
		result = ns_config_get(maps, "max-retry-time", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setmaxretrytime(zone, cfg_obj_asuint32(obj));

		obj = NULL;
		result = ns_config_get(maps, "min-retry-time", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setminretrytime(zone, cfg_obj_asuint32(obj));

		obj = NULL;
		result = ns_config_get(maps, "transfer-source", &obj);
		INSIST(result == ISC_R_SUCCESS);
		RETERR(dns_zone_setxfrsource4(zone, cfg_obj_assockaddr(obj)));
		ns_add_reserved_dispatch(ns_g_server, cfg_obj_assockaddr(obj));

		obj = NULL;
		result = ns_config_get(maps, "transfer-source-v6", &obj);
		INSIST(result == ISC_R_SUCCESS);
		RETERR(dns_zone_setxfrsource6(zone, cfg_obj_assockaddr(obj)));
		ns_add_reserved_dispatch(ns_g_server, cfg_obj_assockaddr(obj));

		obj = NULL;
		result = ns_config_get(maps, "alt-transfer-source", &obj);
		INSIST(result == ISC_R_SUCCESS);
		RETERR(dns_zone_setaltxfrsource4(zone, cfg_obj_assockaddr(obj)));

		obj = NULL;
		result = ns_config_get(maps, "alt-transfer-source-v6", &obj);
		INSIST(result == ISC_R_SUCCESS);
		RETERR(dns_zone_setaltxfrsource6(zone, cfg_obj_assockaddr(obj)));

		obj = NULL;
		(void)ns_config_get(maps, "use-alt-transfer-source", &obj);
		if (obj == NULL) {
			/*
			 * Default off when views are in use otherwise
			 * on for BIND 8 compatibility.
			 */
			view = dns_zone_getview(zone);
			if (view != NULL && strcmp(view->name, "_default") == 0)
				alt = ISC_TRUE;
			else
				alt = ISC_FALSE;
		} else
			alt = cfg_obj_asboolean(obj);
		dns_zone_setoption(zone, DNS_ZONEOPT_USEALTXFRSRC, alt);

		obj = NULL;
		(void)ns_config_get(maps, "try-tcp-refresh", &obj);
		dns_zone_setoption(zone, DNS_ZONEOPT_TRYTCPREFRESH,
				   cfg_obj_asboolean(obj));
		break;

	case dns_zone_staticstub:
		RETERR(configure_staticstub(zoptions, zone, zname,
					    default_dbtype));
		break;

	default:
		break;
	}

	return (ISC_R_SUCCESS);
}


#ifdef DLZ
/*
 * Set up a DLZ zone as writeable
 */
isc_result_t
ns_zone_configure_writeable_dlz(dns_dlzdb_t *dlzdatabase, dns_zone_t *zone,
				dns_rdataclass_t rdclass, dns_name_t *name)
{
	dns_db_t *db = NULL;
	isc_time_t now;
	isc_result_t result;

	TIME_NOW(&now);

	dns_zone_settype(zone, dns_zone_dlz);
	result = dns_sdlz_setdb(dlzdatabase, rdclass, name, &db);
	if (result != ISC_R_SUCCESS)
		return result;
	result = dns_zone_dlzpostload(zone, db);
	dns_db_detach(&db);
	return result;
}
#endif

isc_boolean_t
ns_zone_reusable(dns_zone_t *zone, const cfg_obj_t *zconfig) {
	const cfg_obj_t *zoptions = NULL;
	const cfg_obj_t *obj = NULL;
	const char *cfilename;
	const char *zfilename;

	zoptions = cfg_tuple_get(zconfig, "options");

	if (zonetype_fromconfig(zoptions) != dns_zone_gettype(zone))
		return (ISC_FALSE);

	/*
	 * We always reconfigure a static-stub zone for simplicity, assuming
	 * the amount of data to be loaded is small.
	 */
	if (zonetype_fromconfig(zoptions) == dns_zone_staticstub)
		return (ISC_FALSE);

	obj = NULL;
	(void)cfg_map_get(zoptions, "file", &obj);
	if (obj != NULL)
		cfilename = cfg_obj_asstring(obj);
	else
		cfilename = NULL;
	zfilename = dns_zone_getfile(zone);
	if (!((cfilename == NULL && zfilename == NULL) ||
	      (cfilename != NULL && zfilename != NULL &&
	       strcmp(cfilename, zfilename) == 0)))
	    return (ISC_FALSE);

	return (ISC_TRUE);
}
