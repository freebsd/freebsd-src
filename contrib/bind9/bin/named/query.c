/*
 * Copyright (C) 2004-2007  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: query.c,v 1.257.18.40 2007/09/26 03:08:14 each Exp $ */

/*! \file */

#include <config.h>

#include <string.h>

#include <isc/mem.h>
#include <isc/util.h>

#include <dns/adb.h>
#include <dns/byaddr.h>
#include <dns/db.h>
#ifdef DLZ
#include <dns/dlz.h>
#endif
#include <dns/dnssec.h>
#include <dns/events.h>
#include <dns/message.h>
#include <dns/ncache.h>
#include <dns/order.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/resolver.h>
#include <dns/result.h>
#include <dns/stats.h>
#include <dns/tkey.h>
#include <dns/view.h>
#include <dns/zone.h>
#include <dns/zt.h>

#include <named/client.h>
#include <named/log.h>
#include <named/server.h>
#include <named/sortlist.h>
#include <named/xfrout.h>

/*% Partial answer? */
#define PARTIALANSWER(c)	(((c)->query.attributes & \
				  NS_QUERYATTR_PARTIALANSWER) != 0)
/*% Use Cache? */
#define USECACHE(c)		(((c)->query.attributes & \
				  NS_QUERYATTR_CACHEOK) != 0)
/*% Recursion OK? */
#define RECURSIONOK(c)		(((c)->query.attributes & \
				  NS_QUERYATTR_RECURSIONOK) != 0)
/*% Recursing? */
#define RECURSING(c)		(((c)->query.attributes & \
				  NS_QUERYATTR_RECURSING) != 0)
/*% Cache glue ok? */
#define CACHEGLUEOK(c)		(((c)->query.attributes & \
				  NS_QUERYATTR_CACHEGLUEOK) != 0)
/*% Want Recursion? */
#define WANTRECURSION(c)	(((c)->query.attributes & \
				  NS_QUERYATTR_WANTRECURSION) != 0)
/*% Want DNSSEC? */
#define WANTDNSSEC(c)		(((c)->attributes & \
				  NS_CLIENTATTR_WANTDNSSEC) != 0)
/*% No authority? */
#define NOAUTHORITY(c)		(((c)->query.attributes & \
				  NS_QUERYATTR_NOAUTHORITY) != 0)
/*% No additional? */
#define NOADDITIONAL(c)		(((c)->query.attributes & \
				  NS_QUERYATTR_NOADDITIONAL) != 0)
/*% Secure? */
#define SECURE(c)		(((c)->query.attributes & \
				  NS_QUERYATTR_SECURE) != 0)

#if 0
#define CTRACE(m)       isc_log_write(ns_g_lctx, \
				      NS_LOGCATEGORY_CLIENT, \
				      NS_LOGMODULE_QUERY, \
				      ISC_LOG_DEBUG(3), \
				      "client %p: %s", client, (m))
#define QTRACE(m)       isc_log_write(ns_g_lctx, \
				      NS_LOGCATEGORY_GENERAL, \
				      NS_LOGMODULE_QUERY, \
				      ISC_LOG_DEBUG(3), \
				      "query %p: %s", query, (m))
#else
#define CTRACE(m) ((void)m)
#define QTRACE(m) ((void)m)
#endif

#define DNS_GETDB_NOEXACT 0x01U
#define DNS_GETDB_NOLOG 0x02U
#define DNS_GETDB_PARTIAL 0x04U

#define PENDINGOK(x)	(((x) & DNS_DBFIND_PENDINGOK) != 0)

typedef struct client_additionalctx {
	ns_client_t *client;
	dns_rdataset_t *rdataset;
} client_additionalctx_t;

static void
query_find(ns_client_t *client, dns_fetchevent_t *event, dns_rdatatype_t qtype);

static isc_boolean_t
validate(ns_client_t *client, dns_db_t *db, dns_name_t *name,
	 dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset);

/*%
 * Increment query statistics counters.
 */
static inline void
inc_stats(ns_client_t *client, dns_statscounter_t counter) {
	dns_zone_t *zone = client->query.authzone;

	REQUIRE(counter < DNS_STATS_NCOUNTERS);

	ns_g_server->querystats[counter]++;

	if (zone != NULL) {
		isc_uint64_t *zonestats = dns_zone_getstatscounters(zone);
		if (zonestats != NULL)
			zonestats[counter]++;
	}
}

static void
query_send(ns_client_t *client) {
	dns_statscounter_t counter;
	if (client->message->rcode == dns_rcode_noerror) {
		if (ISC_LIST_EMPTY(client->message->sections[DNS_SECTION_ANSWER])) {
			if (client->query.isreferral) {
				counter = dns_statscounter_referral;
			} else {
				counter = dns_statscounter_nxrrset;
			}
		} else {
			counter = dns_statscounter_success;
		}
	} else if (client->message->rcode == dns_rcode_nxdomain) {
		counter = dns_statscounter_nxdomain;
	} else {
		/* We end up here in case of YXDOMAIN, and maybe others */
		counter = dns_statscounter_failure;
	}
	inc_stats(client, counter);
	ns_client_send(client);
}

static void
query_error(ns_client_t *client, isc_result_t result) {
	inc_stats(client, dns_statscounter_failure);
	ns_client_error(client, result);
}

static void
query_next(ns_client_t *client, isc_result_t result) {
	if (result == DNS_R_DUPLICATE)
		inc_stats(client, dns_statscounter_duplicate);
	else if (result == DNS_R_DROP)
		inc_stats(client, dns_statscounter_dropped);
	else
		inc_stats(client, dns_statscounter_failure);
	ns_client_next(client, result);
}

static inline void
query_freefreeversions(ns_client_t *client, isc_boolean_t everything) {
	ns_dbversion_t *dbversion, *dbversion_next;
	unsigned int i;

	for (dbversion = ISC_LIST_HEAD(client->query.freeversions), i = 0;
	     dbversion != NULL;
	     dbversion = dbversion_next, i++)
	{
		dbversion_next = ISC_LIST_NEXT(dbversion, link);
		/*
		 * If we're not freeing everything, we keep the first three
		 * dbversions structures around.
		 */
		if (i > 3 || everything) {
			ISC_LIST_UNLINK(client->query.freeversions, dbversion,
					link);
			isc_mem_put(client->mctx, dbversion,
				    sizeof(*dbversion));
		}
	}
}

void
ns_query_cancel(ns_client_t *client) {
	LOCK(&client->query.fetchlock);
	if (client->query.fetch != NULL) {
		dns_resolver_cancelfetch(client->query.fetch);

		client->query.fetch = NULL;
	}
	UNLOCK(&client->query.fetchlock);
}

static inline void
query_reset(ns_client_t *client, isc_boolean_t everything) {
	isc_buffer_t *dbuf, *dbuf_next;
	ns_dbversion_t *dbversion, *dbversion_next;

	/*%
	 * Reset the query state of a client to its default state.
	 */

	/*
	 * Cancel the fetch if it's running.
	 */
	ns_query_cancel(client);

	/*
	 * Cleanup any active versions.
	 */
	for (dbversion = ISC_LIST_HEAD(client->query.activeversions);
	     dbversion != NULL;
	     dbversion = dbversion_next) {
		dbversion_next = ISC_LIST_NEXT(dbversion, link);
		dns_db_closeversion(dbversion->db, &dbversion->version,
				    ISC_FALSE);
		dns_db_detach(&dbversion->db);
		ISC_LIST_INITANDAPPEND(client->query.freeversions,
				      dbversion, link);
	}
	ISC_LIST_INIT(client->query.activeversions);

	if (client->query.authdb != NULL)
		dns_db_detach(&client->query.authdb);
	if (client->query.authzone != NULL)
		dns_zone_detach(&client->query.authzone);

	query_freefreeversions(client, everything);

	for (dbuf = ISC_LIST_HEAD(client->query.namebufs);
	     dbuf != NULL;
	     dbuf = dbuf_next) {
		dbuf_next = ISC_LIST_NEXT(dbuf, link);
		if (dbuf_next != NULL || everything) {
			ISC_LIST_UNLINK(client->query.namebufs, dbuf, link);
			isc_buffer_free(&dbuf);
		}
	}

	if (client->query.restarts > 0) {
		/*
		 * client->query.qname was dynamically allocated.
		 */
		dns_message_puttempname(client->message,
					&client->query.qname);
	}
	client->query.qname = NULL;
	client->query.attributes = (NS_QUERYATTR_RECURSIONOK |
				    NS_QUERYATTR_CACHEOK |
				    NS_QUERYATTR_SECURE);
	client->query.restarts = 0;
	client->query.timerset = ISC_FALSE;
	client->query.origqname = NULL;
	client->query.qname = NULL;
	client->query.dboptions = 0;
	client->query.fetchoptions = 0;
	client->query.gluedb = NULL;
	client->query.authdbset = ISC_FALSE;
	client->query.isreferral = ISC_FALSE;
}

static void
query_next_callback(ns_client_t *client) {
	query_reset(client, ISC_FALSE);
}

void
ns_query_free(ns_client_t *client) {
	query_reset(client, ISC_TRUE);
}

static inline isc_result_t
query_newnamebuf(ns_client_t *client) {
	isc_buffer_t *dbuf;
	isc_result_t result;

	CTRACE("query_newnamebuf");
	/*%
	 * Allocate a name buffer.
	 */

	dbuf = NULL;
	result = isc_buffer_allocate(client->mctx, &dbuf, 1024);
	if (result != ISC_R_SUCCESS) {
		CTRACE("query_newnamebuf: isc_buffer_allocate failed: done");
		return (result);
	}
	ISC_LIST_APPEND(client->query.namebufs, dbuf, link);

	CTRACE("query_newnamebuf: done");
	return (ISC_R_SUCCESS);
}

static inline isc_buffer_t *
query_getnamebuf(ns_client_t *client) {
	isc_buffer_t *dbuf;
	isc_result_t result;
	isc_region_t r;

	CTRACE("query_getnamebuf");
	/*%
	 * Return a name buffer with space for a maximal name, allocating
	 * a new one if necessary.
	 */

	if (ISC_LIST_EMPTY(client->query.namebufs)) {
		result = query_newnamebuf(client);
		if (result != ISC_R_SUCCESS) {
		    CTRACE("query_getnamebuf: query_newnamebuf failed: done");
			return (NULL);
		}
	}

	dbuf = ISC_LIST_TAIL(client->query.namebufs);
	INSIST(dbuf != NULL);
	isc_buffer_availableregion(dbuf, &r);
	if (r.length < 255) {
		result = query_newnamebuf(client);
		if (result != ISC_R_SUCCESS) {
		    CTRACE("query_getnamebuf: query_newnamebuf failed: done");
			return (NULL);

		}
		dbuf = ISC_LIST_TAIL(client->query.namebufs);
		isc_buffer_availableregion(dbuf, &r);
		INSIST(r.length >= 255);
	}
	CTRACE("query_getnamebuf: done");
	return (dbuf);
}

static inline void
query_keepname(ns_client_t *client, dns_name_t *name, isc_buffer_t *dbuf) {
	isc_region_t r;

	CTRACE("query_keepname");
	/*%
	 * 'name' is using space in 'dbuf', but 'dbuf' has not yet been
	 * adjusted to take account of that.  We do the adjustment.
	 */

	REQUIRE((client->query.attributes & NS_QUERYATTR_NAMEBUFUSED) != 0);

	dns_name_toregion(name, &r);
	isc_buffer_add(dbuf, r.length);
	dns_name_setbuffer(name, NULL);
	client->query.attributes &= ~NS_QUERYATTR_NAMEBUFUSED;
}

static inline void
query_releasename(ns_client_t *client, dns_name_t **namep) {
	dns_name_t *name = *namep;

	/*%
	 * 'name' is no longer needed.  Return it to our pool of temporary
	 * names.  If it is using a name buffer, relinquish its exclusive
	 * rights on the buffer.
	 */

	CTRACE("query_releasename");
	if (dns_name_hasbuffer(name)) {
		INSIST((client->query.attributes & NS_QUERYATTR_NAMEBUFUSED)
		       != 0);
		client->query.attributes &= ~NS_QUERYATTR_NAMEBUFUSED;
	}
	dns_message_puttempname(client->message, namep);
	CTRACE("query_releasename: done");
}

static inline dns_name_t *
query_newname(ns_client_t *client, isc_buffer_t *dbuf,
	      isc_buffer_t *nbuf)
{
	dns_name_t *name;
	isc_region_t r;
	isc_result_t result;

	REQUIRE((client->query.attributes & NS_QUERYATTR_NAMEBUFUSED) == 0);

	CTRACE("query_newname");
	name = NULL;
	result = dns_message_gettempname(client->message, &name);
	if (result != ISC_R_SUCCESS) {
		CTRACE("query_newname: dns_message_gettempname failed: done");
		return (NULL);
	}
	isc_buffer_availableregion(dbuf, &r);
	isc_buffer_init(nbuf, r.base, r.length);
	dns_name_init(name, NULL);
	dns_name_setbuffer(name, nbuf);
	client->query.attributes |= NS_QUERYATTR_NAMEBUFUSED;

	CTRACE("query_newname: done");
	return (name);
}

static inline dns_rdataset_t *
query_newrdataset(ns_client_t *client) {
	dns_rdataset_t *rdataset;
	isc_result_t result;

	CTRACE("query_newrdataset");
	rdataset = NULL;
	result = dns_message_gettemprdataset(client->message, &rdataset);
	if (result != ISC_R_SUCCESS) {
	  CTRACE("query_newrdataset: "
		 "dns_message_gettemprdataset failed: done");
		return (NULL);
	}
	dns_rdataset_init(rdataset);

	CTRACE("query_newrdataset: done");
	return (rdataset);
}

static inline void
query_putrdataset(ns_client_t *client, dns_rdataset_t **rdatasetp) {
	dns_rdataset_t *rdataset = *rdatasetp;

	CTRACE("query_putrdataset");
	if (rdataset != NULL) {
		if (dns_rdataset_isassociated(rdataset))
			dns_rdataset_disassociate(rdataset);
		dns_message_puttemprdataset(client->message, rdatasetp);
	}
	CTRACE("query_putrdataset: done");
}


static inline isc_result_t
query_newdbversion(ns_client_t *client, unsigned int n) {
	unsigned int i;
	ns_dbversion_t *dbversion;

	for (i = 0; i < n; i++) {
		dbversion = isc_mem_get(client->mctx, sizeof(*dbversion));
		if (dbversion != NULL) {
			dbversion->db = NULL;
			dbversion->version = NULL;
			ISC_LIST_INITANDAPPEND(client->query.freeversions,
					      dbversion, link);
		} else {
			/*
			 * We only return ISC_R_NOMEMORY if we couldn't
			 * allocate anything.
			 */
			if (i == 0)
				return (ISC_R_NOMEMORY);
			else
				return (ISC_R_SUCCESS);
		}
	}

	return (ISC_R_SUCCESS);
}

static inline ns_dbversion_t *
query_getdbversion(ns_client_t *client) {
	isc_result_t result;
	ns_dbversion_t *dbversion;

	if (ISC_LIST_EMPTY(client->query.freeversions)) {
		result = query_newdbversion(client, 1);
		if (result != ISC_R_SUCCESS)
			return (NULL);
	}
	dbversion = ISC_LIST_HEAD(client->query.freeversions);
	INSIST(dbversion != NULL);
	ISC_LIST_UNLINK(client->query.freeversions, dbversion, link);

	return (dbversion);
}

isc_result_t
ns_query_init(ns_client_t *client) {
	isc_result_t result;

	ISC_LIST_INIT(client->query.namebufs);
	ISC_LIST_INIT(client->query.activeversions);
	ISC_LIST_INIT(client->query.freeversions);
	client->query.restarts = 0;
	client->query.timerset = ISC_FALSE;
	client->query.qname = NULL;
	result = isc_mutex_init(&client->query.fetchlock);
	if (result != ISC_R_SUCCESS)
		return (result);
	client->query.fetch = NULL;
	client->query.authdb = NULL;
	client->query.authzone = NULL;
	client->query.authdbset = ISC_FALSE;
	client->query.isreferral = ISC_FALSE;
	query_reset(client, ISC_FALSE);
	result = query_newdbversion(client, 3);
	if (result != ISC_R_SUCCESS) {
		DESTROYLOCK(&client->query.fetchlock);
		return (result);
	}
	result = query_newnamebuf(client);
	if (result != ISC_R_SUCCESS)
		query_freefreeversions(client, ISC_TRUE);

	return (result);
}

static inline ns_dbversion_t *
query_findversion(ns_client_t *client, dns_db_t *db,
		  isc_boolean_t *newzonep)
{
	ns_dbversion_t *dbversion;

	/*%
	 * We may already have done a query related to this
	 * database.  If so, we must be sure to make subsequent
	 * queries from the same version.
	 */
	for (dbversion = ISC_LIST_HEAD(client->query.activeversions);
	     dbversion != NULL;
	     dbversion = ISC_LIST_NEXT(dbversion, link)) {
		if (dbversion->db == db)
			break;
	}

	if (dbversion == NULL) {
		/*
		 * This is a new zone for this query.  Add it to
		 * the active list.
		 */
		dbversion = query_getdbversion(client);
		if (dbversion == NULL)
			return (NULL);
		dns_db_attach(db, &dbversion->db);
		dns_db_currentversion(db, &dbversion->version);
		dbversion->queryok = ISC_FALSE;
		ISC_LIST_APPEND(client->query.activeversions,
				dbversion, link);
		*newzonep = ISC_TRUE;
	} else
		*newzonep = ISC_FALSE;

	return (dbversion);
}

static inline isc_result_t
query_validatezonedb(ns_client_t *client, dns_name_t *name,
		     dns_rdatatype_t qtype, unsigned int options,
		     dns_zone_t *zone, dns_db_t *db,
		     dns_dbversion_t **versionp)
{
	isc_result_t result;
	isc_boolean_t check_acl, new_zone;
	dns_acl_t *queryacl;
	ns_dbversion_t *dbversion;

	REQUIRE(zone != NULL);
	REQUIRE(db != NULL);

	/*
	 * This limits our searching to the zone where the first name
	 * (the query target) was looked for.  This prevents following
	 * CNAMES or DNAMES into other zones and prevents returning
	 * additional data from other zones.
	 */
	if (!client->view->additionalfromauth &&
	    client->query.authdbset &&
	    db != client->query.authdb)
		goto refuse;

	/*
	 * If the zone has an ACL, we'll check it, otherwise
	 * we use the view's "allow-query" ACL.  Each ACL is only checked
	 * once per query.
	 *
	 * Also, get the database version to use.
	 */

	check_acl = ISC_TRUE;	/* Keep compiler happy. */
	queryacl = NULL;

	/*
	 * Get the current version of this database.
	 */
	dbversion = query_findversion(client, db, &new_zone);
	if (dbversion == NULL) {
		result = DNS_R_SERVFAIL;
		goto fail;
	}
	if (new_zone) {
		check_acl = ISC_TRUE;
	} else if (!dbversion->queryok) {
		goto refuse;
	} else {
		check_acl = ISC_FALSE;
	}

	queryacl = dns_zone_getqueryacl(zone);
	if (queryacl == NULL) {
		queryacl = client->view->queryacl;
		if ((client->query.attributes &
		     NS_QUERYATTR_QUERYOKVALID) != 0) {
			/*
			 * We've evaluated the view's queryacl already.  If
			 * NS_QUERYATTR_QUERYOK is set, then the client is
			 * allowed to make queries, otherwise the query should
			 * be refused.
			 */
			check_acl = ISC_FALSE;
			if ((client->query.attributes &
			     NS_QUERYATTR_QUERYOK) == 0)
				goto refuse;
		} else {
			/*
			 * We haven't evaluated the view's queryacl yet.
			 */
			check_acl = ISC_TRUE;
		}
	}

	if (check_acl) {
		isc_boolean_t log = ISC_TF((options & DNS_GETDB_NOLOG) == 0);

		result = ns_client_checkaclsilent(client, queryacl, ISC_TRUE);
		if (log) {
			char msg[NS_CLIENT_ACLMSGSIZE("query")];
			if (result == ISC_R_SUCCESS) {
				if (isc_log_wouldlog(ns_g_lctx,
						     ISC_LOG_DEBUG(3)))
				{
					ns_client_aclmsg("query", name, qtype,
							 client->view->rdclass,
							 msg, sizeof(msg));
					ns_client_log(client,
						      DNS_LOGCATEGORY_SECURITY,
						      NS_LOGMODULE_QUERY,
						      ISC_LOG_DEBUG(3),
						      "%s approved", msg);
				}
			} else {
				ns_client_aclmsg("query", name, qtype,
						 client->view->rdclass,
						 msg, sizeof(msg));
				ns_client_log(client, DNS_LOGCATEGORY_SECURITY,
					      NS_LOGMODULE_QUERY, ISC_LOG_INFO,
					      "%s denied", msg);
			}
		}

		if (queryacl == client->view->queryacl) {
			if (result == ISC_R_SUCCESS) {
				/*
				 * We were allowed by the default
				 * "allow-query" ACL.  Remember this so we
				 * don't have to check again.
				 */
				client->query.attributes |=
					NS_QUERYATTR_QUERYOK;
			}
			/*
			 * We've now evaluated the view's query ACL, and
			 * the NS_QUERYATTR_QUERYOK attribute is now valid.
			 */
			client->query.attributes |= NS_QUERYATTR_QUERYOKVALID;
		}

		if (result != ISC_R_SUCCESS)
			goto refuse;
	}

	/* Approved. */

	/*
	 * Remember the result of the ACL check so we
	 * don't have to check again.
	 */
	dbversion->queryok = ISC_TRUE;

	/* Transfer ownership, if necessary. */
	if (versionp != NULL)
		*versionp = dbversion->version;

	return (ISC_R_SUCCESS);

 refuse:
	return (DNS_R_REFUSED);

 fail:
	return (result);
}

static inline isc_result_t
query_getzonedb(ns_client_t *client, dns_name_t *name, dns_rdatatype_t qtype,
		unsigned int options, dns_zone_t **zonep, dns_db_t **dbp,
		dns_dbversion_t **versionp)
{
	isc_result_t result;
	unsigned int ztoptions;
	dns_zone_t *zone = NULL;
	dns_db_t *db = NULL;
	isc_boolean_t partial = ISC_FALSE;

	REQUIRE(zonep != NULL && *zonep == NULL);
	REQUIRE(dbp != NULL && *dbp == NULL);

	/*%
	 * Find a zone database to answer the query.
	 */
	ztoptions = ((options & DNS_GETDB_NOEXACT) != 0) ?
		DNS_ZTFIND_NOEXACT : 0;

	result = dns_zt_find(client->view->zonetable, name, ztoptions, NULL,
			     &zone);
	if (result == DNS_R_PARTIALMATCH)
		partial = ISC_TRUE;
	if (result == ISC_R_SUCCESS || result == DNS_R_PARTIALMATCH)
		result = dns_zone_getdb(zone, &db);

	if (result != ISC_R_SUCCESS)
		goto fail;

	result = query_validatezonedb(client, name, qtype, options, zone, db,
				      versionp);

	if (result != ISC_R_SUCCESS)
		goto fail;

	/* Transfer ownership. */
	*zonep = zone;
	*dbp = db;

	if (partial && (options & DNS_GETDB_PARTIAL) != 0)
		return (DNS_R_PARTIALMATCH);
	return (ISC_R_SUCCESS);

 fail:
	if (zone != NULL)
		dns_zone_detach(&zone);
	if (db != NULL)
		dns_db_detach(&db);

	return (result);
}

static inline isc_result_t
query_getcachedb(ns_client_t *client, dns_name_t *name, dns_rdatatype_t qtype,
		 dns_db_t **dbp, unsigned int options)
{
	isc_result_t result;
	isc_boolean_t check_acl;
	dns_db_t *db = NULL;

	REQUIRE(dbp != NULL && *dbp == NULL);

	/*%
	 * Find a cache database to answer the query.
	 * This may fail with DNS_R_REFUSED if the client
	 * is not allowed to use the cache.
	 */

	if (!USECACHE(client))
		return (DNS_R_REFUSED);
	dns_db_attach(client->view->cachedb, &db);

	if ((client->query.attributes &
	     NS_QUERYATTR_QUERYOKVALID) != 0) {
		/*
		 * We've evaluated the view's queryacl already.  If
		 * NS_QUERYATTR_QUERYOK is set, then the client is
		 * allowed to make queries, otherwise the query should
		 * be refused.
		 */
		check_acl = ISC_FALSE;
		if ((client->query.attributes &
		     NS_QUERYATTR_QUERYOK) == 0)
			goto refuse;
	} else {
		/*
		 * We haven't evaluated the view's queryacl yet.
		 */
		check_acl = ISC_TRUE;
	}

	if (check_acl) {
		isc_boolean_t log = ISC_TF((options & DNS_GETDB_NOLOG) == 0);
		char msg[NS_CLIENT_ACLMSGSIZE("query (cache)")];

		result = ns_client_checkaclsilent(client,
						  client->view->queryacl,
						  ISC_TRUE);
		if (result == ISC_R_SUCCESS) {
			/*
			 * We were allowed by the default
			 * "allow-query" ACL.  Remember this so we
			 * don't have to check again.
			 */
			client->query.attributes |=
				NS_QUERYATTR_QUERYOK;
			if (log && isc_log_wouldlog(ns_g_lctx,
						     ISC_LOG_DEBUG(3)))
			{
				ns_client_aclmsg("query (cache)", name, qtype,
						 client->view->rdclass,
						 msg, sizeof(msg));
				ns_client_log(client,
					      DNS_LOGCATEGORY_SECURITY,
					      NS_LOGMODULE_QUERY,
					      ISC_LOG_DEBUG(3),
					      "%s approved", msg);
			}
		} else if (log) {
			ns_client_aclmsg("query (cache)", name, qtype,
					 client->view->rdclass, msg,
					 sizeof(msg));
			ns_client_log(client, DNS_LOGCATEGORY_SECURITY,
				      NS_LOGMODULE_QUERY, ISC_LOG_INFO,
				      "%s denied", msg);
		}
		/*
		 * We've now evaluated the view's query ACL, and
		 * the NS_QUERYATTR_QUERYOK attribute is now valid.
		 */
		client->query.attributes |= NS_QUERYATTR_QUERYOKVALID;

		if (result != ISC_R_SUCCESS)
			goto refuse;
	}

	/* Approved. */

	/* Transfer ownership. */
	*dbp = db;

	return (ISC_R_SUCCESS);

 refuse:
	result = DNS_R_REFUSED;

	if (db != NULL)
		dns_db_detach(&db);

	return (result);
}


static inline isc_result_t
query_getdb(ns_client_t *client, dns_name_t *name, dns_rdatatype_t qtype,
	    unsigned int options, dns_zone_t **zonep, dns_db_t **dbp,
	    dns_dbversion_t **versionp, isc_boolean_t *is_zonep)
{
	isc_result_t result;

#ifdef DLZ
	isc_result_t tresult;
	unsigned int namelabels;
	unsigned int zonelabels;
	dns_zone_t *zone = NULL;
	dns_db_t *tdbp;

	REQUIRE(zonep != NULL && *zonep == NULL);

	tdbp = NULL;

	/* Calculate how many labels are in name. */
	namelabels = dns_name_countlabels(name);
	zonelabels = 0;

	/* Try to find name in bind's standard database. */
	result = query_getzonedb(client, name, qtype, options, &zone,
				 dbp, versionp);

	/* See how many labels are in the zone's name.	  */
	if (result == ISC_R_SUCCESS && zone != NULL)
		zonelabels = dns_name_countlabels(dns_zone_getorigin(zone));
	/*
	 * If # zone labels < # name labels, try to find an even better match
	 * Only try if a DLZ driver is loaded for this view
	 */
	if (zonelabels < namelabels && client->view->dlzdatabase != NULL) {
		tresult = dns_dlzfindzone(client->view, name,
					  zonelabels, &tdbp);
		 /* If we successful, we found a better match. */
		if (tresult == ISC_R_SUCCESS) {
			/*
			 * If the previous search returned a zone, detach it.
			 */
			if (zone != NULL)
				dns_zone_detach(&zone);

			/*
			 * If the previous search returned a database,
			 * detach it.
			 */
			if (*dbp != NULL)
				dns_db_detach(dbp);

			/*
			 * If the previous search returned a version, clear it.
			 */
			*versionp = NULL;

			/*
			 * Get our database version.
			 */
			dns_db_currentversion(tdbp, versionp);

			/*
			 * Be sure to return our database.
			 */
			*dbp = tdbp;

			/*
			 * We return a null zone, No stats for DLZ zones.
			 */
			zone = NULL;
			result = tresult;
		}
	}
#else
	result = query_getzonedb(client, name, qtype, options,
				 zonep, dbp, versionp);
#endif

	/* If successfull, Transfer ownership of zone. */
	if (result == ISC_R_SUCCESS) {
#ifdef DLZ
		*zonep = zone;
#endif
		/*
		 * If neither attempt above succeeded, return the cache instead
		 */
		*is_zonep = ISC_TRUE;
	} else if (result == ISC_R_NOTFOUND) {
		result = query_getcachedb(client, name, qtype, dbp, options);
		*is_zonep = ISC_FALSE;
	}
	return (result);
}

static inline isc_boolean_t
query_isduplicate(ns_client_t *client, dns_name_t *name,
		  dns_rdatatype_t type, dns_name_t **mnamep)
{
	dns_section_t section;
	dns_name_t *mname = NULL;
	isc_result_t result;

	CTRACE("query_isduplicate");

	for (section = DNS_SECTION_ANSWER;
	     section <= DNS_SECTION_ADDITIONAL;
	     section++) {
		result = dns_message_findname(client->message, section,
					      name, type, 0, &mname, NULL);
		if (result == ISC_R_SUCCESS) {
			/*
			 * We've already got this RRset in the response.
			 */
			CTRACE("query_isduplicate: true: done");
			return (ISC_TRUE);
		} else if (result == DNS_R_NXRRSET) {
			/*
			 * The name exists, but the rdataset does not.
			 */
			if (section == DNS_SECTION_ADDITIONAL)
				break;
		} else
			RUNTIME_CHECK(result == DNS_R_NXDOMAIN);
		mname = NULL;
	}

	/*
	 * If the dns_name_t we're looking up is already in the message,
	 * we don't want to trigger the caller's name replacement logic.
	 */
	if (name == mname)
		mname = NULL;

	*mnamep = mname;

	CTRACE("query_isduplicate: false: done");
	return (ISC_FALSE);
}

static isc_result_t
query_addadditional(void *arg, dns_name_t *name, dns_rdatatype_t qtype) {
	ns_client_t *client = arg;
	isc_result_t result, eresult;
	dns_dbnode_t *node;
	dns_db_t *db;
	dns_name_t *fname, *mname;
	dns_rdataset_t *rdataset, *sigrdataset, *trdataset;
	isc_buffer_t *dbuf;
	isc_buffer_t b;
	dns_dbversion_t *version;
	isc_boolean_t added_something, need_addname;
	dns_zone_t *zone;
	dns_rdatatype_t type;

	REQUIRE(NS_CLIENT_VALID(client));
	REQUIRE(qtype != dns_rdatatype_any);

	if (!WANTDNSSEC(client) && dns_rdatatype_isdnssec(qtype))
		return (ISC_R_SUCCESS);

	CTRACE("query_addadditional");

	/*
	 * Initialization.
	 */
	eresult = ISC_R_SUCCESS;
	fname = NULL;
	rdataset = NULL;
	sigrdataset = NULL;
	trdataset = NULL;
	db = NULL;
	version = NULL;
	node = NULL;
	added_something = ISC_FALSE;
	need_addname = ISC_FALSE;
	zone = NULL;

	/*
	 * We treat type A additional section processing as if it
	 * were "any address type" additional section processing.
	 * To avoid multiple lookups, we do an 'any' database
	 * lookup and iterate over the node.
	 */
	if (qtype == dns_rdatatype_a)
		type = dns_rdatatype_any;
	else
		type = qtype;

	/*
	 * Get some resources.
	 */
	dbuf = query_getnamebuf(client);
	if (dbuf == NULL)
		goto cleanup;
	fname = query_newname(client, dbuf, &b);
	rdataset = query_newrdataset(client);
	if (fname == NULL || rdataset == NULL)
		goto cleanup;
	if (WANTDNSSEC(client)) {
		sigrdataset = query_newrdataset(client);
		if (sigrdataset == NULL)
			goto cleanup;
	}

	/*
	 * Look for a zone database that might contain authoritative
	 * additional data.
	 */
	result = query_getzonedb(client, name, qtype, DNS_GETDB_NOLOG,
				 &zone, &db, &version);
	if (result != ISC_R_SUCCESS)
		goto try_cache;

	CTRACE("query_addadditional: db_find");

	/*
	 * Since we are looking for authoritative data, we do not set
	 * the GLUEOK flag.  Glue will be looked for later, but not
	 * necessarily in the same database.
	 */
	node = NULL;
	result = dns_db_find(db, name, version, type, client->query.dboptions,
			     client->now, &node, fname, rdataset,
			     sigrdataset);
	if (result == ISC_R_SUCCESS)
		goto found;

	if (dns_rdataset_isassociated(rdataset))
		dns_rdataset_disassociate(rdataset);
	if (sigrdataset != NULL && dns_rdataset_isassociated(sigrdataset))
		dns_rdataset_disassociate(sigrdataset);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	version = NULL;
	dns_db_detach(&db);

	/*
	 * No authoritative data was found.  The cache is our next best bet.
	 */

 try_cache:
	result = query_getcachedb(client, name, qtype, &db, DNS_GETDB_NOLOG);
	if (result != ISC_R_SUCCESS)
		/*
		 * Most likely the client isn't allowed to query the cache.
		 */
		goto try_glue;
	/*
	 * Attempt to validate glue.
	 */
	if (sigrdataset == NULL) {
		sigrdataset = query_newrdataset(client);
		if (sigrdataset == NULL)
			goto cleanup;
	}
	result = dns_db_find(db, name, version, type,
			     client->query.dboptions | DNS_DBFIND_GLUEOK,
			     client->now, &node, fname, rdataset,
			     sigrdataset);
	if (result == DNS_R_GLUE &&
	    validate(client, db, fname, rdataset, sigrdataset))
		result = ISC_R_SUCCESS;
	if (!WANTDNSSEC(client))
		query_putrdataset(client, &sigrdataset);
	if (result == ISC_R_SUCCESS)
		goto found;

	if (dns_rdataset_isassociated(rdataset))
		dns_rdataset_disassociate(rdataset);
	if (sigrdataset != NULL && dns_rdataset_isassociated(sigrdataset))
		dns_rdataset_disassociate(sigrdataset);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	dns_db_detach(&db);

 try_glue:
	/*
	 * No cached data was found.  Glue is our last chance.
	 * RFC1035 sayeth:
	 *
	 *	NS records cause both the usual additional section
	 *	processing to locate a type A record, and, when used
	 *	in a referral, a special search of the zone in which
	 *	they reside for glue information.
	 *
	 * This is the "special search".  Note that we must search
	 * the zone where the NS record resides, not the zone it
	 * points to, and that we only do the search in the delegation
	 * case (identified by client->query.gluedb being set).
	 */

	if (client->query.gluedb == NULL)
		goto cleanup;

	/*
	 * Don't poision caches using the bailiwick protection model.
	 */
	if (!dns_name_issubdomain(name, dns_db_origin(client->query.gluedb)))
		goto cleanup;

	dns_db_attach(client->query.gluedb, &db);
	result = dns_db_find(db, name, version, type,
			     client->query.dboptions | DNS_DBFIND_GLUEOK,
			     client->now, &node, fname, rdataset,
			     sigrdataset);
	if (!(result == ISC_R_SUCCESS ||
	      result == DNS_R_ZONECUT ||
	      result == DNS_R_GLUE))
		goto cleanup;

 found:
	/*
	 * We have found a potential additional data rdataset, or
	 * at least a node to iterate over.
	 */
	query_keepname(client, fname, dbuf);

	/*
	 * If we have an rdataset, add it to the additional data
	 * section.
	 */
	mname = NULL;
	if (dns_rdataset_isassociated(rdataset) &&
	    !query_isduplicate(client, fname, type, &mname)) {
		if (mname != NULL) {
			query_releasename(client, &fname);
			fname = mname;
		} else
			need_addname = ISC_TRUE;
		ISC_LIST_APPEND(fname->list, rdataset, link);
		trdataset = rdataset;
		rdataset = NULL;
		added_something = ISC_TRUE;
		/*
		 * Note: we only add SIGs if we've added the type they cover,
		 * so we do not need to check if the SIG rdataset is already
		 * in the response.
		 */
		if (sigrdataset != NULL &&
		    dns_rdataset_isassociated(sigrdataset))
		{
			ISC_LIST_APPEND(fname->list, sigrdataset, link);
			sigrdataset = NULL;
		}
	}

	if (qtype == dns_rdatatype_a) {
		/*
		 * We now go looking for A and AAAA records, along with
		 * their signatures.
		 *
		 * XXXRTH  This code could be more efficient.
		 */
		if (rdataset != NULL) {
			if (dns_rdataset_isassociated(rdataset))
				dns_rdataset_disassociate(rdataset);
		} else {
			rdataset = query_newrdataset(client);
			if (rdataset == NULL)
				goto addname;
		}
		if (sigrdataset != NULL) {
			if (dns_rdataset_isassociated(sigrdataset))
				dns_rdataset_disassociate(sigrdataset);
		} else if (WANTDNSSEC(client)) {
			sigrdataset = query_newrdataset(client);
			if (sigrdataset == NULL)
				goto addname;
		}
		result = dns_db_findrdataset(db, node, version,
					     dns_rdatatype_a, 0,
					     client->now, rdataset,
					     sigrdataset);
		if (result == DNS_R_NCACHENXDOMAIN)
			goto addname;
		if (result == DNS_R_NCACHENXRRSET) {
			dns_rdataset_disassociate(rdataset);
			/*
			 * Negative cache entries don't have sigrdatasets.
			 */
			INSIST(sigrdataset == NULL ||
			       ! dns_rdataset_isassociated(sigrdataset));
		}
		if (result == ISC_R_SUCCESS) {
			mname = NULL;
			if (!query_isduplicate(client, fname,
					       dns_rdatatype_a, &mname)) {
				if (mname != NULL) {
					query_releasename(client, &fname);
					fname = mname;
				} else
					need_addname = ISC_TRUE;
				ISC_LIST_APPEND(fname->list, rdataset, link);
				added_something = ISC_TRUE;
				if (sigrdataset != NULL &&
				    dns_rdataset_isassociated(sigrdataset))
				{
					ISC_LIST_APPEND(fname->list,
							sigrdataset, link);
					sigrdataset =
						query_newrdataset(client);
				}
				rdataset = query_newrdataset(client);
				if (rdataset == NULL)
					goto addname;
				if (WANTDNSSEC(client) && sigrdataset == NULL)
					goto addname;
			} else {
				dns_rdataset_disassociate(rdataset);
				if (sigrdataset != NULL &&
				    dns_rdataset_isassociated(sigrdataset))
					dns_rdataset_disassociate(sigrdataset);
			}
		}
		result = dns_db_findrdataset(db, node, version,
					     dns_rdatatype_aaaa, 0,
					     client->now, rdataset,
					     sigrdataset);
		if (result == DNS_R_NCACHENXDOMAIN)
			goto addname;
		if (result == DNS_R_NCACHENXRRSET) {
			dns_rdataset_disassociate(rdataset);
			INSIST(sigrdataset == NULL ||
			       ! dns_rdataset_isassociated(sigrdataset));
		}
		if (result == ISC_R_SUCCESS) {
			mname = NULL;
			if (!query_isduplicate(client, fname,
					       dns_rdatatype_aaaa, &mname)) {
				if (mname != NULL) {
					query_releasename(client, &fname);
					fname = mname;
				} else
					need_addname = ISC_TRUE;
				ISC_LIST_APPEND(fname->list, rdataset, link);
				added_something = ISC_TRUE;
				if (sigrdataset != NULL &&
				    dns_rdataset_isassociated(sigrdataset))
				{
					ISC_LIST_APPEND(fname->list,
							sigrdataset, link);
					sigrdataset = NULL;
				}
				rdataset = NULL;
			}
		}
	}

 addname:
	CTRACE("query_addadditional: addname");
	/*
	 * If we haven't added anything, then we're done.
	 */
	if (!added_something)
		goto cleanup;

	/*
	 * We may have added our rdatasets to an existing name, if so, then
	 * need_addname will be ISC_FALSE.  Whether we used an existing name
	 * or a new one, we must set fname to NULL to prevent cleanup.
	 */
	if (need_addname)
		dns_message_addname(client->message, fname,
				    DNS_SECTION_ADDITIONAL);
	fname = NULL;

	/*
	 * In a few cases, we want to add additional data for additional
	 * data.  It's simpler to just deal with special cases here than
	 * to try to create a general purpose mechanism and allow the
	 * rdata implementations to do it themselves.
	 *
	 * This involves recursion, but the depth is limited.  The
	 * most complex case is adding a SRV rdataset, which involves
	 * recursing to add address records, which in turn can cause
	 * recursion to add KEYs.
	 */
	if (type == dns_rdatatype_srv && trdataset != NULL) {
		/*
		 * If we're adding SRV records to the additional data
		 * section, it's helpful if we add the SRV additional data
		 * as well.
		 */
		eresult = dns_rdataset_additionaldata(trdataset,
						      query_addadditional,
						      client);
	}

 cleanup:
	CTRACE("query_addadditional: cleanup");
	query_putrdataset(client, &rdataset);
	if (sigrdataset != NULL)
		query_putrdataset(client, &sigrdataset);
	if (fname != NULL)
		query_releasename(client, &fname);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (db != NULL)
		dns_db_detach(&db);
	if (zone != NULL)
		dns_zone_detach(&zone);

	CTRACE("query_addadditional: done");
	return (eresult);
}

static inline void
query_discardcache(ns_client_t *client, dns_rdataset_t *rdataset_base,
		   dns_rdatasetadditional_t additionaltype,
		   dns_rdatatype_t type, dns_zone_t **zonep, dns_db_t **dbp,
		   dns_dbversion_t **versionp, dns_dbnode_t **nodep,
		   dns_name_t *fname)
{
	dns_rdataset_t *rdataset;

	while  ((rdataset = ISC_LIST_HEAD(fname->list)) != NULL) {
		ISC_LIST_UNLINK(fname->list, rdataset, link);
		query_putrdataset(client, &rdataset);
	}
	if (*versionp != NULL)
		dns_db_closeversion(*dbp, versionp, ISC_FALSE);
	if (*nodep != NULL)
		dns_db_detachnode(*dbp, nodep);
	if (*dbp != NULL)
		dns_db_detach(dbp);
	if (*zonep != NULL)
		dns_zone_detach(zonep);
	(void)dns_rdataset_putadditional(client->view->acache, rdataset_base,
					 additionaltype, type);
}

static inline isc_result_t
query_iscachevalid(dns_zone_t *zone, dns_db_t *db, dns_db_t *db0,
		   dns_dbversion_t *version)
{
	isc_result_t result = ISC_R_SUCCESS;
	dns_dbversion_t *version_current = NULL;
	dns_db_t *db_current = db0;

	if (db_current == NULL) {
		result = dns_zone_getdb(zone, &db_current);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	dns_db_currentversion(db_current, &version_current);
	if (db_current != db || version_current != version) {
		result = ISC_R_FAILURE;
		goto cleanup;
	}

 cleanup:
	dns_db_closeversion(db_current, &version_current, ISC_FALSE);
	if (db0 == NULL && db_current != NULL)
		dns_db_detach(&db_current);

	return (result);
}

static isc_result_t
query_addadditional2(void *arg, dns_name_t *name, dns_rdatatype_t qtype) {
	client_additionalctx_t *additionalctx = arg;
	dns_rdataset_t *rdataset_base;
	ns_client_t *client;
	isc_result_t result, eresult;
	dns_dbnode_t *node, *cnode;
	dns_db_t *db, *cdb;
	dns_name_t *fname, *mname0, cfname;
	dns_rdataset_t *rdataset, *sigrdataset;
	dns_rdataset_t *crdataset, *crdataset_next;
	isc_buffer_t *dbuf;
	isc_buffer_t b;
	dns_dbversion_t *version, *cversion;
	isc_boolean_t added_something, need_addname, needadditionalcache;
	isc_boolean_t need_sigrrset;
	dns_zone_t *zone;
	dns_rdatatype_t type;
	dns_rdatasetadditional_t additionaltype;

	if (qtype != dns_rdatatype_a) {
		/*
		 * This function is optimized for "address" types.  For other
		 * types, use a generic routine.
		 * XXX: ideally, this function should be generic enough.
		 */
		return (query_addadditional(additionalctx->client,
					    name, qtype));
	}

	/*
	 * Initialization.
	 */
	rdataset_base = additionalctx->rdataset;
	client = additionalctx->client;
	REQUIRE(NS_CLIENT_VALID(client));
	eresult = ISC_R_SUCCESS;
	fname = NULL;
	rdataset = NULL;
	sigrdataset = NULL;
	db = NULL;
	cdb = NULL;
	version = NULL;
	cversion = NULL;
	node = NULL;
	cnode = NULL;
	added_something = ISC_FALSE;
	need_addname = ISC_FALSE;
	zone = NULL;
	needadditionalcache = ISC_FALSE;
	additionaltype = dns_rdatasetadditional_fromauth;
	dns_name_init(&cfname, NULL);

	CTRACE("query_addadditional2");

	/*
	 * We treat type A additional section processing as if it
	 * were "any address type" additional section processing.
	 * To avoid multiple lookups, we do an 'any' database
	 * lookup and iterate over the node.
	 * XXXJT: this approach can cause a suboptimal result when the cache
	 * DB only has partial address types and the glue DB has remaining
	 * ones.
	 */
	type = dns_rdatatype_any;

	/*
	 * Get some resources.
	 */
	dbuf = query_getnamebuf(client);
	if (dbuf == NULL)
		goto cleanup;
	fname = query_newname(client, dbuf, &b);
	if (fname == NULL)
		goto cleanup;
	dns_name_setbuffer(&cfname, &b); /* share the buffer */

	/* Check additional cache */
	result = dns_rdataset_getadditional(rdataset_base, additionaltype,
					    type, client->view->acache, &zone,
					    &cdb, &cversion, &cnode, &cfname,
					    client->message, client->now);
	if (result != ISC_R_SUCCESS)
		goto findauthdb;
	if (zone == NULL) {
		CTRACE("query_addadditional2: auth zone not found");
		goto try_cache;
	}

	/* Is the cached DB up-to-date? */
	result = query_iscachevalid(zone, cdb, NULL, cversion);
	if (result != ISC_R_SUCCESS) {
		CTRACE("query_addadditional2: old auth additional cache");
		query_discardcache(client, rdataset_base, additionaltype,
				   type, &zone, &cdb, &cversion, &cnode,
				   &cfname);
		goto findauthdb;
	}

	if (cnode == NULL) {
		/*
		 * We have a negative cache.  We don't have to check the zone
		 * ACL, since the result (not using this zone) would be same
		 * regardless of the result.
		 */
		CTRACE("query_addadditional2: negative auth additional cache");
		dns_db_closeversion(cdb, &cversion, ISC_FALSE);
		dns_db_detach(&cdb);
		dns_zone_detach(&zone);
		goto try_cache;
	}

	result = query_validatezonedb(client, name, qtype, DNS_GETDB_NOLOG,
				      zone, cdb, NULL);
	if (result != ISC_R_SUCCESS) {
		query_discardcache(client, rdataset_base, additionaltype,
				   type, &zone, &cdb, &cversion, &cnode,
				   &cfname);
		goto try_cache;
	}

	/* We've got an active cache. */
	CTRACE("query_addadditional2: auth additional cache");
	dns_db_closeversion(cdb, &cversion, ISC_FALSE);
	db = cdb;
	node = cnode;
	dns_name_clone(&cfname, fname);
	query_keepname(client, fname, dbuf);
	goto foundcache;

	/*
	 * Look for a zone database that might contain authoritative
	 * additional data.
	 */
 findauthdb:
	result = query_getzonedb(client, name, qtype, DNS_GETDB_NOLOG,
				 &zone, &db, &version);
	if (result != ISC_R_SUCCESS) {
		/* Cache the negative result */
		(void)dns_rdataset_setadditional(rdataset_base, additionaltype,
						 type, client->view->acache,
						 NULL, NULL, NULL, NULL,
						 NULL);
		goto try_cache;
	}

	CTRACE("query_addadditional2: db_find");

	/*
	 * Since we are looking for authoritative data, we do not set
	 * the GLUEOK flag.  Glue will be looked for later, but not
	 * necessarily in the same database.
	 */
	node = NULL;
	result = dns_db_find(db, name, version, type, client->query.dboptions,
			     client->now, &node, fname, NULL, NULL);
	if (result == ISC_R_SUCCESS)
		goto found;

	/* Cache the negative result */
	(void)dns_rdataset_setadditional(rdataset_base, additionaltype,
					 type, client->view->acache, zone, db,
					 version, NULL, fname);

	if (node != NULL)
		dns_db_detachnode(db, &node);
	version = NULL;
	dns_db_detach(&db);

	/*
	 * No authoritative data was found.  The cache is our next best bet.
	 */

 try_cache:
	additionaltype = dns_rdatasetadditional_fromcache;
	result = query_getcachedb(client, name, qtype, &db, DNS_GETDB_NOLOG);
	if (result != ISC_R_SUCCESS)
		/*
		 * Most likely the client isn't allowed to query the cache.
		 */
		goto try_glue;

	result = dns_db_find(db, name, version, type,
			     client->query.dboptions | DNS_DBFIND_GLUEOK,
			     client->now, &node, fname, NULL, NULL);
	if (result == ISC_R_SUCCESS)
		goto found;

	if (node != NULL)
		dns_db_detachnode(db, &node);
	dns_db_detach(&db);

 try_glue:
	/*
	 * No cached data was found.  Glue is our last chance.
	 * RFC1035 sayeth:
	 *
	 *	NS records cause both the usual additional section
	 *	processing to locate a type A record, and, when used
	 *	in a referral, a special search of the zone in which
	 *	they reside for glue information.
	 *
	 * This is the "special search".  Note that we must search
	 * the zone where the NS record resides, not the zone it
	 * points to, and that we only do the search in the delegation
	 * case (identified by client->query.gluedb being set).
	 */
	if (client->query.gluedb == NULL)
		goto cleanup;

	/*
	 * Don't poision caches using the bailiwick protection model.
	 */
	if (!dns_name_issubdomain(name, dns_db_origin(client->query.gluedb)))
		goto cleanup;

	/* Check additional cache */
	additionaltype = dns_rdatasetadditional_fromglue;
	result = dns_rdataset_getadditional(rdataset_base, additionaltype,
					    type, client->view->acache, NULL,
					    &cdb, &cversion, &cnode, &cfname,
					    client->message, client->now);
	if (result != ISC_R_SUCCESS)
		goto findglue;

	result = query_iscachevalid(zone, cdb, client->query.gluedb, cversion);
	if (result != ISC_R_SUCCESS) {
		CTRACE("query_addadditional2: old glue additional cache");
		query_discardcache(client, rdataset_base, additionaltype,
				   type, &zone, &cdb, &cversion, &cnode,
				   &cfname);
		goto findglue;
	}

	if (cnode == NULL) {
		/* We have a negative cache. */
		CTRACE("query_addadditional2: negative glue additional cache");
		dns_db_closeversion(cdb, &cversion, ISC_FALSE);
		dns_db_detach(&cdb);
		goto cleanup;
	}

	/* Cache hit. */
	CTRACE("query_addadditional2: glue additional cache");
	dns_db_closeversion(cdb, &cversion, ISC_FALSE);
	db = cdb;
	node = cnode;
	dns_name_clone(&cfname, fname);
	query_keepname(client, fname, dbuf);
	goto foundcache;

 findglue:
	dns_db_attach(client->query.gluedb, &db);
	result = dns_db_find(db, name, version, type,
			     client->query.dboptions | DNS_DBFIND_GLUEOK,
			     client->now, &node, fname, NULL, NULL);
	if (!(result == ISC_R_SUCCESS ||
	      result == DNS_R_ZONECUT ||
	      result == DNS_R_GLUE)) {
		/* cache the negative result */
		(void)dns_rdataset_setadditional(rdataset_base, additionaltype,
						 type, client->view->acache,
						 NULL, db, version, NULL,
						 fname);
		goto cleanup;
	}

 found:
	/*
	 * We have found a DB node to iterate over from a DB.
	 * We are going to look for address RRsets (i.e., A and AAAA) in the DB
	 * node we've just found.  We'll then store the complete information
	 * in the additional data cache.
	 */
	dns_name_clone(fname, &cfname);
	query_keepname(client, fname, dbuf);
	needadditionalcache = ISC_TRUE;

	rdataset = query_newrdataset(client);
	if (rdataset == NULL)
		goto cleanup;

	sigrdataset = query_newrdataset(client);
	if (sigrdataset == NULL)
		goto cleanup;

	/*
	 * Find A RRset with sig RRset.  Even if we don't find a sig RRset
	 * for a client using DNSSEC, we'll continue the process to make a
	 * complete list to be cached.  However, we need to cancel the
	 * caching when something unexpected happens, in order to avoid
	 * caching incomplete information.
	 */
	result = dns_db_findrdataset(db, node, version, dns_rdatatype_a, 0,
				     client->now, rdataset, sigrdataset);
	/*
	 * If we can't promote glue/pending from the cache to secure
	 * then drop it.
	 */
	if (result == ISC_R_SUCCESS &&
	    additionaltype == dns_rdatasetadditional_fromcache &&
	    (DNS_TRUST_PENDING(rdataset->trust) ||
	     DNS_TRUST_GLUE(rdataset->trust)) &&
	    !validate(client, db, fname, rdataset, sigrdataset)) {
		dns_rdataset_disassociate(rdataset);
		if (dns_rdataset_isassociated(sigrdataset))
			dns_rdataset_disassociate(sigrdataset);
		result = ISC_R_NOTFOUND;
	}
	if (result == DNS_R_NCACHENXDOMAIN)
		goto setcache;
	if (result == DNS_R_NCACHENXRRSET) {
		dns_rdataset_disassociate(rdataset);
		/*
		 * Negative cache entries don't have sigrdatasets.
		 */
		INSIST(! dns_rdataset_isassociated(sigrdataset));
	}
	if (result == ISC_R_SUCCESS) {
		/* Remember the result as a cache */
		ISC_LIST_APPEND(cfname.list, rdataset, link);
		if (dns_rdataset_isassociated(sigrdataset)) {
			ISC_LIST_APPEND(cfname.list, sigrdataset, link);
			sigrdataset = query_newrdataset(client);
		}
		rdataset = query_newrdataset(client);
		if (sigrdataset == NULL || rdataset == NULL) {
			/* do not cache incomplete information */
			goto foundcache;
		}
	}

	/* Find AAAA RRset with sig RRset */
	result = dns_db_findrdataset(db, node, version, dns_rdatatype_aaaa,
				     0, client->now, rdataset, sigrdataset);
	/*
	 * If we can't promote glue/pending from the cache to secure
	 * then drop it.
	 */
	if (result == ISC_R_SUCCESS &&
	    additionaltype == dns_rdatasetadditional_fromcache &&
	    (DNS_TRUST_PENDING(rdataset->trust) ||
	     DNS_TRUST_GLUE(rdataset->trust)) &&
	    !validate(client, db, fname, rdataset, sigrdataset)) {
		dns_rdataset_disassociate(rdataset);
		if (dns_rdataset_isassociated(sigrdataset))
			dns_rdataset_disassociate(sigrdataset);
		result = ISC_R_NOTFOUND;
	}
	if (result == ISC_R_SUCCESS) {
		ISC_LIST_APPEND(cfname.list, rdataset, link);
		rdataset = NULL;
		if (dns_rdataset_isassociated(sigrdataset)) {
			ISC_LIST_APPEND(cfname.list, sigrdataset, link);
			sigrdataset = NULL;
		}
	}

 setcache:
	/*
	 * Set the new result in the cache if required.  We do not support
	 * caching additional data from a cache DB.
	 */
	if (needadditionalcache == ISC_TRUE &&
	    (additionaltype == dns_rdatasetadditional_fromauth ||
	     additionaltype == dns_rdatasetadditional_fromglue)) {
		(void)dns_rdataset_setadditional(rdataset_base, additionaltype,
						 type, client->view->acache,
						 zone, db, version, node,
						 &cfname);
	}

 foundcache:
	need_sigrrset = ISC_FALSE;
	mname0 = NULL;
	for (crdataset = ISC_LIST_HEAD(cfname.list);
	     crdataset != NULL;
	     crdataset = crdataset_next) {
		dns_name_t *mname;

		crdataset_next = ISC_LIST_NEXT(crdataset, link);

		mname = NULL;
		if (crdataset->type == dns_rdatatype_a ||
		    crdataset->type == dns_rdatatype_aaaa) {
			if (!query_isduplicate(client, fname, crdataset->type,
					       &mname)) {
				if (mname != NULL) {
					/*
					 * A different type of this name is
					 * already stored in the additional
					 * section.  We'll reuse the name.
					 * Note that this should happen at most
					 * once.  Otherwise, fname->link could
					 * leak below.
					 */
					INSIST(mname0 == NULL);

					query_releasename(client, &fname);
					fname = mname;
					mname0 = mname;
				} else
					need_addname = ISC_TRUE;
				ISC_LIST_UNLINK(cfname.list, crdataset, link);
				ISC_LIST_APPEND(fname->list, crdataset, link);
				added_something = ISC_TRUE;
				need_sigrrset = ISC_TRUE;
			} else
				need_sigrrset = ISC_FALSE;
		} else if (crdataset->type == dns_rdatatype_rrsig &&
			   need_sigrrset && WANTDNSSEC(client)) {
			ISC_LIST_UNLINK(cfname.list, crdataset, link);
			ISC_LIST_APPEND(fname->list, crdataset, link);
			added_something = ISC_TRUE; /* just in case */
			need_sigrrset = ISC_FALSE;
		}
	}

	CTRACE("query_addadditional2: addname");

	/*
	 * If we haven't added anything, then we're done.
	 */
	if (!added_something)
		goto cleanup;

	/*
	 * We may have added our rdatasets to an existing name, if so, then
	 * need_addname will be ISC_FALSE.  Whether we used an existing name
	 * or a new one, we must set fname to NULL to prevent cleanup.
	 */
	if (need_addname)
		dns_message_addname(client->message, fname,
				    DNS_SECTION_ADDITIONAL);
	fname = NULL;

 cleanup:
	CTRACE("query_addadditional2: cleanup");

	if (rdataset != NULL)
		query_putrdataset(client, &rdataset);
	if (sigrdataset != NULL)
		query_putrdataset(client, &sigrdataset);
	while  ((crdataset = ISC_LIST_HEAD(cfname.list)) != NULL) {
		ISC_LIST_UNLINK(cfname.list, crdataset, link);
		query_putrdataset(client, &crdataset);
	}
	if (fname != NULL)
		query_releasename(client, &fname);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (db != NULL)
		dns_db_detach(&db);
	if (zone != NULL)
		dns_zone_detach(&zone);

	CTRACE("query_addadditional2: done");
	return (eresult);
}

static inline void
query_addrdataset(ns_client_t *client, dns_name_t *fname,
		  dns_rdataset_t *rdataset)
{
	client_additionalctx_t additionalctx;

	/*
	 * Add 'rdataset' and any pertinent additional data to
	 * 'fname', a name in the response message for 'client'.
	 */

	CTRACE("query_addrdataset");

	ISC_LIST_APPEND(fname->list, rdataset, link);

	if (client->view->order != NULL)
		rdataset->attributes |= dns_order_find(client->view->order,
						       fname, rdataset->type,
						       rdataset->rdclass);
	rdataset->attributes |= DNS_RDATASETATTR_LOADORDER;

	if (NOADDITIONAL(client))
		return;

	/*
	 * Add additional data.
	 *
	 * We don't care if dns_rdataset_additionaldata() fails.
	 */
	additionalctx.client = client;
	additionalctx.rdataset = rdataset;
	(void)dns_rdataset_additionaldata(rdataset, query_addadditional2,
					  &additionalctx);
	CTRACE("query_addrdataset: done");
}

static void
query_addrrset(ns_client_t *client, dns_name_t **namep,
	       dns_rdataset_t **rdatasetp, dns_rdataset_t **sigrdatasetp,
	       isc_buffer_t *dbuf, dns_section_t section)
{
	dns_name_t *name, *mname;
	dns_rdataset_t *rdataset, *mrdataset, *sigrdataset;
	isc_result_t result;

	/*%
	 * To the current response for 'client', add the answer RRset
	 * '*rdatasetp' and an optional signature set '*sigrdatasetp', with
	 * owner name '*namep', to section 'section', unless they are
	 * already there.  Also add any pertinent additional data.
	 *
	 * If 'dbuf' is not NULL, then '*namep' is the name whose data is
	 * stored in 'dbuf'.  In this case, query_addrrset() guarantees that
	 * when it returns the name will either have been kept or released.
	 */
	CTRACE("query_addrrset");
	name = *namep;
	rdataset = *rdatasetp;
	if (sigrdatasetp != NULL)
		sigrdataset = *sigrdatasetp;
	else
		sigrdataset = NULL;
	mname = NULL;
	mrdataset = NULL;
	result = dns_message_findname(client->message, section,
				      name, rdataset->type, rdataset->covers,
				      &mname, &mrdataset);
	if (result == ISC_R_SUCCESS) {
		/*
		 * We've already got an RRset of the given name and type.
		 * There's nothing else to do;
		 */
		CTRACE("query_addrrset: dns_message_findname succeeded: done");
		if (dbuf != NULL)
			query_releasename(client, namep);
		return;
	} else if (result == DNS_R_NXDOMAIN) {
		/*
		 * The name doesn't exist.
		 */
		if (dbuf != NULL)
			query_keepname(client, name, dbuf);
		dns_message_addname(client->message, name, section);
		*namep = NULL;
		mname = name;
	} else {
		RUNTIME_CHECK(result == DNS_R_NXRRSET);
		if (dbuf != NULL)
			query_releasename(client, namep);
	}

	if (rdataset->trust != dns_trust_secure &&
	    (section == DNS_SECTION_ANSWER ||
	     section == DNS_SECTION_AUTHORITY))
		client->query.attributes &= ~NS_QUERYATTR_SECURE;
	/*
	 * Note: we only add SIGs if we've added the type they cover, so
	 * we do not need to check if the SIG rdataset is already in the
	 * response.
	 */
	query_addrdataset(client, mname, rdataset);
	*rdatasetp = NULL;
	if (sigrdataset != NULL && dns_rdataset_isassociated(sigrdataset)) {
		/*
		 * We have a signature.  Add it to the response.
		 */
		ISC_LIST_APPEND(mname->list, sigrdataset, link);
		*sigrdatasetp = NULL;
	}
	CTRACE("query_addrrset: done");
}

static inline isc_result_t
query_addsoa(ns_client_t *client, dns_db_t *db, dns_dbversion_t *version,
	     isc_boolean_t zero_ttl)
{
	dns_name_t *name;
	dns_dbnode_t *node;
	isc_result_t result, eresult;
	dns_rdataset_t *rdataset = NULL, *sigrdataset = NULL;
	dns_rdataset_t **sigrdatasetp = NULL;

	CTRACE("query_addsoa");
	/*
	 * Initialization.
	 */
	eresult = ISC_R_SUCCESS;
	name = NULL;
	rdataset = NULL;
	node = NULL;

	/*
	 * Get resources and make 'name' be the database origin.
	 */
	result = dns_message_gettempname(client->message, &name);
	if (result != ISC_R_SUCCESS)
		return (result);
	dns_name_init(name, NULL);
	dns_name_clone(dns_db_origin(db), name);
	rdataset = query_newrdataset(client);
	if (rdataset == NULL) {
		eresult = DNS_R_SERVFAIL;
		goto cleanup;
	}
	if (WANTDNSSEC(client)) {
		sigrdataset = query_newrdataset(client);
		if (sigrdataset == NULL) {
			eresult = DNS_R_SERVFAIL;
			goto cleanup;
		}
	}

	/*
	 * Find the SOA.
	 */
	result = dns_db_getoriginnode(db, &node);
	if (result == ISC_R_SUCCESS) {
		result = dns_db_findrdataset(db, node, version,
					     dns_rdatatype_soa,
					     0, client->now, rdataset,
					     sigrdataset);
	} else {
		dns_fixedname_t foundname;
		dns_name_t *fname;

		dns_fixedname_init(&foundname);
		fname = dns_fixedname_name(&foundname);

		result = dns_db_find(db, name, version, dns_rdatatype_soa,
				     client->query.dboptions, 0, &node,
				     fname, rdataset, sigrdataset);
	}
	if (result != ISC_R_SUCCESS) {
		/*
		 * This is bad.  We tried to get the SOA RR at the zone top
		 * and it didn't work!
		 */
		eresult = DNS_R_SERVFAIL;
	} else {
		/*
		 * Extract the SOA MINIMUM.
		 */
		dns_rdata_soa_t soa;
		dns_rdata_t rdata = DNS_RDATA_INIT;
		result = dns_rdataset_first(rdataset);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		dns_rdataset_current(rdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &soa, NULL);
		if (result != ISC_R_SUCCESS)
			goto cleanup;

		if (zero_ttl) {
			rdataset->ttl = 0;
			if (sigrdataset != NULL)
				sigrdataset->ttl = 0;
		}

		/*
		 * Add the SOA and its SIG to the response, with the
		 * TTLs adjusted per RFC2308 section 3.
		 */
		if (rdataset->ttl > soa.minimum)
			rdataset->ttl = soa.minimum;
		if (sigrdataset != NULL && sigrdataset->ttl > soa.minimum)
			sigrdataset->ttl = soa.minimum;

		if (sigrdataset != NULL)
			sigrdatasetp = &sigrdataset;
		else
			sigrdatasetp = NULL;
		query_addrrset(client, &name, &rdataset, sigrdatasetp, NULL,
			       DNS_SECTION_AUTHORITY);
	}

 cleanup:
	query_putrdataset(client, &rdataset);
	if (sigrdataset != NULL)
		query_putrdataset(client, &sigrdataset);
	if (name != NULL)
		query_releasename(client, &name);
	if (node != NULL)
		dns_db_detachnode(db, &node);

	return (eresult);
}

static inline isc_result_t
query_addns(ns_client_t *client, dns_db_t *db, dns_dbversion_t *version) {
	dns_name_t *name, *fname;
	dns_dbnode_t *node;
	isc_result_t result, eresult;
	dns_fixedname_t foundname;
	dns_rdataset_t *rdataset = NULL, *sigrdataset = NULL;
	dns_rdataset_t **sigrdatasetp = NULL;

	CTRACE("query_addns");
	/*
	 * Initialization.
	 */
	eresult = ISC_R_SUCCESS;
	name = NULL;
	rdataset = NULL;
	node = NULL;
	dns_fixedname_init(&foundname);
	fname = dns_fixedname_name(&foundname);

	/*
	 * Get resources and make 'name' be the database origin.
	 */
	result = dns_message_gettempname(client->message, &name);
	if (result != ISC_R_SUCCESS) {
		CTRACE("query_addns: dns_message_gettempname failed: done");
		return (result);
	}
	dns_name_init(name, NULL);
	dns_name_clone(dns_db_origin(db), name);
	rdataset = query_newrdataset(client);
	if (rdataset == NULL) {
		CTRACE("query_addns: query_newrdataset failed");
		eresult = DNS_R_SERVFAIL;
		goto cleanup;
	}
	if (WANTDNSSEC(client)) {
		sigrdataset = query_newrdataset(client);
		if (sigrdataset == NULL) {
			CTRACE("query_addns: query_newrdataset failed");
			eresult = DNS_R_SERVFAIL;
			goto cleanup;
		}
	}

	/*
	 * Find the NS rdataset.
	 */
	result = dns_db_getoriginnode(db, &node);
	if (result == ISC_R_SUCCESS) {
		result = dns_db_findrdataset(db, node, version,
					     dns_rdatatype_ns,
					     0, client->now, rdataset,
					     sigrdataset);
	} else {
		CTRACE("query_addns: calling dns_db_find");
		result = dns_db_find(db, name, NULL, dns_rdatatype_ns,
				     client->query.dboptions, 0, &node,
				     fname, rdataset, sigrdataset);
		CTRACE("query_addns: dns_db_find complete");
	}
	if (result != ISC_R_SUCCESS) {
		CTRACE("query_addns: "
		       "dns_db_findrdataset or dns_db_find failed");
		/*
		 * This is bad.  We tried to get the NS rdataset at the zone
		 * top and it didn't work!
		 */
		eresult = DNS_R_SERVFAIL;
	} else {
		if (sigrdataset != NULL)
			sigrdatasetp = &sigrdataset;
		else
			sigrdatasetp = NULL;
		query_addrrset(client, &name, &rdataset, sigrdatasetp, NULL,
			       DNS_SECTION_AUTHORITY);
	}

 cleanup:
	CTRACE("query_addns: cleanup");
	query_putrdataset(client, &rdataset);
	if (sigrdataset != NULL)
		query_putrdataset(client, &sigrdataset);
	if (name != NULL)
		query_releasename(client, &name);
	if (node != NULL)
		dns_db_detachnode(db, &node);

	CTRACE("query_addns: done");
	return (eresult);
}

static inline isc_result_t
query_addcnamelike(ns_client_t *client, dns_name_t *qname, dns_name_t *tname,
		   dns_trust_t trust, dns_name_t **anamep, dns_rdatatype_t type)
{
	dns_rdataset_t *rdataset;
	dns_rdatalist_t *rdatalist;
	dns_rdata_t *rdata;
	isc_result_t result;
	isc_region_t r;

	/*
	 * We assume the name data referred to by tname won't go away.
	 */

	REQUIRE(anamep != NULL);

	rdatalist = NULL;
	result = dns_message_gettemprdatalist(client->message, &rdatalist);
	if (result != ISC_R_SUCCESS)
		return (result);
	rdata = NULL;
	result = dns_message_gettemprdata(client->message, &rdata);
	if (result != ISC_R_SUCCESS)
		return (result);
	rdataset = NULL;
	result = dns_message_gettemprdataset(client->message, &rdataset);
	if (result != ISC_R_SUCCESS)
		return (result);
	dns_rdataset_init(rdataset);
	result = dns_name_dup(qname, client->mctx, *anamep);
	if (result != ISC_R_SUCCESS) {
		dns_message_puttemprdataset(client->message, &rdataset);
		return (result);
	}

	rdatalist->type = type;
	rdatalist->covers = 0;
	rdatalist->rdclass = client->message->rdclass;
	rdatalist->ttl = 0;

	dns_name_toregion(tname, &r);
	rdata->data = r.base;
	rdata->length = r.length;
	rdata->rdclass = client->message->rdclass;
	rdata->type = type;

	ISC_LIST_INIT(rdatalist->rdata);
	ISC_LIST_APPEND(rdatalist->rdata, rdata, link);
	RUNTIME_CHECK(dns_rdatalist_tordataset(rdatalist, rdataset)
		      == ISC_R_SUCCESS);
	rdataset->trust = trust;

	query_addrrset(client, anamep, &rdataset, NULL, NULL,
		       DNS_SECTION_ANSWER);

	if (rdataset != NULL) {
		if (dns_rdataset_isassociated(rdataset))
			dns_rdataset_disassociate(rdataset);
		dns_message_puttemprdataset(client->message, &rdataset);
	}

	return (ISC_R_SUCCESS);
}

/*
 * Mark the RRsets as secure.  Update the cache (db) to reflect the
 * change in trust level.
 */
static void
mark_secure(ns_client_t *client, dns_db_t *db, dns_name_t *name,
	    dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset)
{
	isc_result_t result;
	dns_dbnode_t *node = NULL;

	rdataset->trust = dns_trust_secure;
	sigrdataset->trust = dns_trust_secure;

	/*
	 * Save the updated secure state.  Ignore failures.
	 */
	result = dns_db_findnode(db, name, ISC_TRUE, &node);
	if (result != ISC_R_SUCCESS)
		return;
	(void)dns_db_addrdataset(db, node, NULL, client->now, rdataset,
				 0, NULL);
	(void)dns_db_addrdataset(db, node, NULL, client->now, sigrdataset,
				 0, NULL);
	dns_db_detachnode(db, &node);
}

/*
 * Find the secure key that corresponds to rrsig.
 * Note: 'keyrdataset' maintains state between sucessive calls,
 * there may be multiple keys with the same keyid.
 * Return ISC_FALSE if we have exhausted all the possible keys.
 */
static isc_boolean_t
get_key(ns_client_t *client, dns_db_t *db, dns_rdata_rrsig_t *rrsig,
	dns_rdataset_t *keyrdataset, dst_key_t **keyp)
{ 
	isc_result_t result;
	dns_dbnode_t *node = NULL;
	isc_boolean_t secure = ISC_FALSE;

	if (!dns_rdataset_isassociated(keyrdataset)) {
		result = dns_db_findnode(db, &rrsig->signer, ISC_FALSE, &node);
		if (result != ISC_R_SUCCESS)
			return (ISC_FALSE);

		result = dns_db_findrdataset(db, node, NULL,
					     dns_rdatatype_dnskey, 0,
					     client->now, keyrdataset, NULL);
		dns_db_detachnode(db, &node);
		if (result != ISC_R_SUCCESS)
			return (ISC_FALSE);

		if (keyrdataset->trust != dns_trust_secure)
			return (ISC_FALSE);

		result = dns_rdataset_first(keyrdataset);
	} else
		result = dns_rdataset_next(keyrdataset);

	for ( ; result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(keyrdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;
		isc_buffer_t b;

		dns_rdataset_current(keyrdataset, &rdata);
		isc_buffer_init(&b, rdata.data, rdata.length);
		isc_buffer_add(&b, rdata.length);
		result = dst_key_fromdns(&rrsig->signer, rdata.rdclass, &b,
                                         client->mctx, keyp);
		if (result != ISC_R_SUCCESS)
			continue;
		if (rrsig->algorithm == (dns_secalg_t)dst_key_alg(*keyp) &&
                    rrsig->keyid == (dns_keytag_t)dst_key_id(*keyp) &&
                    dst_key_iszonekey(*keyp)) {
			secure = ISC_TRUE;
			break;
		}
		dst_key_free(keyp);
	}
	return (secure);
}

static isc_boolean_t
verify(dst_key_t *key, dns_name_t *name, dns_rdataset_t *rdataset,
       dns_rdata_t *rdata, isc_mem_t *mctx, isc_boolean_t acceptexpired)
{
	isc_result_t result;
	dns_fixedname_t fixed;
	isc_boolean_t ignore = ISC_FALSE;

	dns_fixedname_init(&fixed);
	
again:
	result = dns_dnssec_verify2(name, rdataset, key, ignore, mctx,
				    rdata, NULL);
	if (result == DNS_R_SIGEXPIRED && acceptexpired) {
		ignore = ISC_TRUE;
		goto again;
	}
	if (result == ISC_R_SUCCESS || result == DNS_R_FROMWILDCARD)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

/*
 * Validate the rdataset if possible with available records.
 */
static isc_boolean_t
validate(ns_client_t *client, dns_db_t *db, dns_name_t *name,
	 dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset)
{
	isc_result_t result;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_rrsig_t rrsig;
	dst_key_t *key = NULL;
	dns_rdataset_t keyrdataset;

	if (sigrdataset == NULL || !dns_rdataset_isassociated(sigrdataset))
		return (ISC_FALSE);
	
	for (result = dns_rdataset_first(sigrdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(sigrdataset)) {

		dns_rdata_reset(&rdata);
		dns_rdataset_current(sigrdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &rrsig, NULL);
		if (result != ISC_R_SUCCESS)
			return (ISC_FALSE);
		if (!dns_resolver_algorithm_supported(client->view->resolver,
						      name, rrsig.algorithm))
			continue;
		if (!dns_name_issubdomain(name, &rrsig.signer))
			continue;
		dns_rdataset_init(&keyrdataset);
		do {
			if (!get_key(client, db, &rrsig, &keyrdataset, &key))
				break;
			if (verify(key, name, rdataset, &rdata, client->mctx,
				   client->view->acceptexpired)) {
				dst_key_free(&key);
				dns_rdataset_disassociate(&keyrdataset);
				mark_secure(client, db, name, rdataset,
					    sigrdataset);
				return (ISC_TRUE);
			}
			dst_key_free(&key);
		} while (1);
		if (dns_rdataset_isassociated(&keyrdataset))
			dns_rdataset_disassociate(&keyrdataset);
	}
	return (ISC_FALSE);
}

static void
query_addbestns(ns_client_t *client) {
	dns_db_t *db, *zdb;
	dns_dbnode_t *node;
	dns_name_t *fname, *zfname;
	dns_rdataset_t *rdataset, *sigrdataset, *zrdataset, *zsigrdataset;
	isc_boolean_t is_zone, use_zone;
	isc_buffer_t *dbuf;
	isc_result_t result;
	dns_dbversion_t *version;
	dns_zone_t *zone;
	isc_buffer_t b;

	CTRACE("query_addbestns");
	fname = NULL;
	zfname = NULL;
	rdataset = NULL;
	zrdataset = NULL;
	sigrdataset = NULL;
	zsigrdataset = NULL;
	node = NULL;
	db = NULL;
	zdb = NULL;
	version = NULL;
	zone = NULL;
	is_zone = ISC_FALSE;
	use_zone = ISC_FALSE;

	/*
	 * Find the right database.
	 */
	result = query_getdb(client, client->query.qname, dns_rdatatype_ns, 0,
			     &zone, &db, &version, &is_zone);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

 db_find:
	/*
	 * We'll need some resources...
	 */
	dbuf = query_getnamebuf(client);
	if (dbuf == NULL)
		goto cleanup;
	fname = query_newname(client, dbuf, &b);
	rdataset = query_newrdataset(client);
	if (fname == NULL || rdataset == NULL)
		goto cleanup;
	/*
	 * Get the RRSIGs if the client requested them or if we may
	 * need to validate answers from the cache.
	 */
	if (WANTDNSSEC(client) || !is_zone) {
		sigrdataset = query_newrdataset(client);
		if (sigrdataset == NULL)
			goto cleanup;
	}

	/*
	 * Now look for the zonecut.
	 */
	if (is_zone) {
		result = dns_db_find(db, client->query.qname, version,
				     dns_rdatatype_ns, client->query.dboptions,
				     client->now, &node, fname,
				     rdataset, sigrdataset);
		if (result != DNS_R_DELEGATION)
			goto cleanup;
		if (USECACHE(client)) {
			query_keepname(client, fname, dbuf);
			zdb = db;
			zfname = fname;
			fname = NULL;
			zrdataset = rdataset;
			rdataset = NULL;
			zsigrdataset = sigrdataset;
			sigrdataset = NULL;
			dns_db_detachnode(db, &node);
			version = NULL;
			db = NULL;
			dns_db_attach(client->view->cachedb, &db);
			is_zone = ISC_FALSE;
			goto db_find;
		}
	} else {
		result = dns_db_findzonecut(db, client->query.qname,
					    client->query.dboptions,
					    client->now, &node, fname,
					    rdataset, sigrdataset);
		if (result == ISC_R_SUCCESS) {
			if (zfname != NULL &&
			    !dns_name_issubdomain(fname, zfname)) {
				/*
				 * We found a zonecut in the cache, but our
				 * zone delegation is better.
				 */
				use_zone = ISC_TRUE;
			}
		} else if (result == ISC_R_NOTFOUND && zfname != NULL) {
			/*
			 * We didn't find anything in the cache, but we
			 * have a zone delegation, so use it.
			 */
			use_zone = ISC_TRUE;
		} else
			goto cleanup;
	}

	if (use_zone) {
		query_releasename(client, &fname);
		fname = zfname;
		zfname = NULL;
		/*
		 * We've already done query_keepname() on
		 * zfname, so we must set dbuf to NULL to
		 * prevent query_addrrset() from trying to
		 * call query_keepname() again.
		 */
		dbuf = NULL;
		query_putrdataset(client, &rdataset);
		if (sigrdataset != NULL)
			query_putrdataset(client, &sigrdataset);
		rdataset = zrdataset;
		zrdataset = NULL;
		sigrdataset = zsigrdataset;
		zsigrdataset = NULL;
	}

	/*
	 * Attempt to validate RRsets that are pending or that are glue.
	 */
	if ((DNS_TRUST_PENDING(rdataset->trust) ||
	     (sigrdataset != NULL && DNS_TRUST_PENDING(sigrdataset->trust)))
	    && !validate(client, db, fname, rdataset, sigrdataset) &&
	    !PENDINGOK(client->query.dboptions))
		goto cleanup;

	if ((DNS_TRUST_GLUE(rdataset->trust) ||
	     (sigrdataset != NULL && DNS_TRUST_GLUE(sigrdataset->trust))) &&
	    !validate(client, db, fname, rdataset, sigrdataset) &&
	    SECURE(client) && WANTDNSSEC(client))
		goto cleanup;

	/*
	 * If the client doesn't want DNSSEC we can discard the sigrdataset
	 * now.
	 */
	if (!WANTDNSSEC(client))
		query_putrdataset(client, &sigrdataset);
	query_addrrset(client, &fname, &rdataset, &sigrdataset, dbuf,
		       DNS_SECTION_AUTHORITY);

 cleanup:
	if (rdataset != NULL)
		query_putrdataset(client, &rdataset);
	if (sigrdataset != NULL)
		query_putrdataset(client, &sigrdataset);
	if (fname != NULL)
		query_releasename(client, &fname);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (db != NULL)
		dns_db_detach(&db);
	if (zone != NULL)
		dns_zone_detach(&zone);
	if (zdb != NULL) {
		query_putrdataset(client, &zrdataset);
		if (zsigrdataset != NULL)
			query_putrdataset(client, &zsigrdataset);
		if (zfname != NULL)
			query_releasename(client, &zfname);
		dns_db_detach(&zdb);
	}
}

static void
query_addds(ns_client_t *client, dns_db_t *db, dns_dbnode_t *node,
	    dns_dbversion_t *version)
{
	dns_name_t *rname;
	dns_rdataset_t *rdataset, *sigrdataset;
	isc_result_t result;

	CTRACE("query_addds");
	rname = NULL;
	rdataset = NULL;
	sigrdataset = NULL;

	/*
	 * We'll need some resources...
	 */
	rdataset = query_newrdataset(client);
	sigrdataset = query_newrdataset(client);
	if (rdataset == NULL || sigrdataset == NULL)
		goto cleanup;

	/*
	 * Look for the DS record, which may or may not be present.
	 */
	result = dns_db_findrdataset(db, node, version, dns_rdatatype_ds, 0,
				     client->now, rdataset, sigrdataset);
	/*
	 * If we didn't find it, look for an NSEC. */
	if (result == ISC_R_NOTFOUND)
		result = dns_db_findrdataset(db, node, version,
					     dns_rdatatype_nsec, 0, client->now,
					     rdataset, sigrdataset);
	if (result != ISC_R_SUCCESS && result != ISC_R_NOTFOUND)
		goto cleanup;
	if (!dns_rdataset_isassociated(rdataset) ||
	    !dns_rdataset_isassociated(sigrdataset))
		goto cleanup;

	/*
	 * We've already added the NS record, so if the name's not there,
	 * we have other problems.  Use this name rather than calling
	 * query_addrrset().
	 */
	result = dns_message_firstname(client->message, DNS_SECTION_AUTHORITY);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	rname = NULL;
	dns_message_currentname(client->message, DNS_SECTION_AUTHORITY,
				&rname);
	result = dns_message_findtype(rname, dns_rdatatype_ns, 0, NULL);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	ISC_LIST_APPEND(rname->list, rdataset, link);
	ISC_LIST_APPEND(rname->list, sigrdataset, link);
	rdataset = NULL;
	sigrdataset = NULL;

 cleanup:
	if (rdataset != NULL)
		query_putrdataset(client, &rdataset);
	if (sigrdataset != NULL)
		query_putrdataset(client, &sigrdataset);
}

static void
query_addwildcardproof(ns_client_t *client, dns_db_t *db,
		       dns_dbversion_t *version, dns_name_t *name,
		       isc_boolean_t ispositive)
{
	isc_buffer_t *dbuf, b;
	dns_name_t *fname;
	dns_rdataset_t *rdataset, *sigrdataset;
	dns_fixedname_t wfixed;
	dns_name_t *wname;
	dns_dbnode_t *node;
	unsigned int options;
	unsigned int olabels, nlabels;
	isc_result_t result;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_nsec_t nsec;
	isc_boolean_t have_wname;
	int order;

	CTRACE("query_addwildcardproof");
	fname = NULL;
	rdataset = NULL;
	sigrdataset = NULL;
	node = NULL;

	/*
	 * Get the NOQNAME proof then if !ispositve
	 * get the NOWILDCARD proof.
	 *
	 * DNS_DBFIND_NOWILD finds the NSEC records that covers the
	 * name ignoring any wildcard.  From the owner and next names
	 * of this record you can compute which wildcard (if it exists)
	 * will match by finding the longest common suffix of the
	 * owner name and next names with the qname and prefixing that
	 * with the wildcard label.
	 *
	 * e.g.
	 *   Given:
	 *	example SOA
	 *	example NSEC b.example
	 *	b.example A
	 *	b.example NSEC a.d.example
	 *	a.d.example A
	 *	a.d.example NSEC g.f.example
	 *	g.f.example A
	 *	g.f.example NSEC z.i.example
	 *	z.i.example A
	 *	z.i.example NSEC example
	 *
	 *   QNAME:
	 *   a.example -> example NSEC b.example
	 *	owner common example
	 *	next common example
	 *	wild *.example
	 *   d.b.example -> b.example NSEC a.d.example
	 *	owner common b.example
	 *	next common example
	 *	wild *.b.example
	 *   a.f.example -> a.d.example NSEC g.f.example
	 *	owner common example
	 *	next common f.example
	 *	wild *.f.example
	 *  j.example -> z.i.example NSEC example
	 *	owner common example
	 *	next common example
	 *	wild *.f.example
	 */
	options = client->query.dboptions | DNS_DBFIND_NOWILD;
	dns_fixedname_init(&wfixed);
	wname = dns_fixedname_name(&wfixed);
 again:
	have_wname = ISC_FALSE;
	/*
	 * We'll need some resources...
	 */
	dbuf = query_getnamebuf(client);
	if (dbuf == NULL)
		goto cleanup;
	fname = query_newname(client, dbuf, &b);
	rdataset = query_newrdataset(client);
	sigrdataset = query_newrdataset(client);
	if (fname == NULL || rdataset == NULL || sigrdataset == NULL)
		goto cleanup;

	result = dns_db_find(db, name, version, dns_rdatatype_nsec, options,
			     0, &node, fname, rdataset, sigrdataset);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (result == DNS_R_NXDOMAIN) {
		if (!ispositive)
			result = dns_rdataset_first(rdataset);
		if (result == ISC_R_SUCCESS) {
			dns_rdataset_current(rdataset, &rdata);
			result = dns_rdata_tostruct(&rdata, &nsec, NULL);
		}
		if (result == ISC_R_SUCCESS) {
			(void)dns_name_fullcompare(name, fname, &order,
						   &olabels);
			(void)dns_name_fullcompare(name, &nsec.next, &order,
						   &nlabels);
			if (olabels > nlabels)
				dns_name_split(name, olabels, NULL, wname);
			else
				dns_name_split(name, nlabels, NULL, wname);
			result = dns_name_concatenate(dns_wildcardname,
						      wname, wname, NULL);
			if (result == ISC_R_SUCCESS)
				have_wname = ISC_TRUE;
			dns_rdata_freestruct(&nsec);
		}
		query_addrrset(client, &fname, &rdataset, &sigrdataset,
			       dbuf, DNS_SECTION_AUTHORITY);
	}
	if (rdataset != NULL)
		query_putrdataset(client, &rdataset);
	if (sigrdataset != NULL)
		query_putrdataset(client, &sigrdataset);
	if (fname != NULL)
		query_releasename(client, &fname);
	if (have_wname) {
		ispositive = ISC_TRUE;	/* prevent loop */
		if (!dns_name_equal(name, wname)) {
			name = wname;
			goto again;
		}
	}
 cleanup:
	if (rdataset != NULL)
		query_putrdataset(client, &rdataset);
	if (sigrdataset != NULL)
		query_putrdataset(client, &sigrdataset);
	if (fname != NULL)
		query_releasename(client, &fname);
}

static void
query_addnxrrsetnsec(ns_client_t *client, dns_db_t *db,
		     dns_dbversion_t *version, dns_name_t **namep,
		     dns_rdataset_t **rdatasetp, dns_rdataset_t **sigrdatasetp)
{
	dns_name_t *name;
	dns_rdataset_t *sigrdataset;
	dns_rdata_t sigrdata;
	dns_rdata_rrsig_t sig;
	unsigned int labels;
	isc_buffer_t *dbuf, b;
	dns_name_t *fname;
	isc_result_t result;

	name = *namep;
	if ((name->attributes & DNS_NAMEATTR_WILDCARD) == 0) {
		query_addrrset(client, namep, rdatasetp, sigrdatasetp,
			       NULL, DNS_SECTION_AUTHORITY);
		return;
	}

	if (sigrdatasetp == NULL)
		return;
	sigrdataset = *sigrdatasetp;
	if (sigrdataset == NULL || !dns_rdataset_isassociated(sigrdataset))
		return;
	result = dns_rdataset_first(sigrdataset);
	if (result != ISC_R_SUCCESS)
		return;
	dns_rdata_init(&sigrdata);
	dns_rdataset_current(sigrdataset, &sigrdata);
	result = dns_rdata_tostruct(&sigrdata, &sig, NULL);
	if (result != ISC_R_SUCCESS)
		return;

	labels = dns_name_countlabels(name);
	if ((unsigned int)sig.labels + 1 >= labels)
		return;

	/* XXX */
	query_addwildcardproof(client, db, version, client->query.qname,
			       ISC_TRUE);

	/*
	 * We'll need some resources...
	 */
	dbuf = query_getnamebuf(client);
	if (dbuf == NULL)
		return;
	fname = query_newname(client, dbuf, &b);
	if (fname == NULL)
		return;
	dns_name_split(name, sig.labels + 1, NULL, fname);
	/* This will succeed, since we've stripped labels. */
	RUNTIME_CHECK(dns_name_concatenate(dns_wildcardname, fname, fname,
					   NULL) == ISC_R_SUCCESS);
	query_addrrset(client, &fname, rdatasetp, sigrdatasetp,
		       dbuf, DNS_SECTION_AUTHORITY);
}

static void
query_resume(isc_task_t *task, isc_event_t *event) {
	dns_fetchevent_t *devent = (dns_fetchevent_t *)event;
	ns_client_t *client;
	isc_boolean_t fetch_cancelled, client_shuttingdown;

	/*
	 * Resume a query after recursion.
	 */

	UNUSED(task);

	REQUIRE(event->ev_type == DNS_EVENT_FETCHDONE);
	client = devent->ev_arg;
	REQUIRE(NS_CLIENT_VALID(client));
	REQUIRE(task == client->task);
	REQUIRE(RECURSING(client));

	LOCK(&client->query.fetchlock);
	if (client->query.fetch != NULL) {
		/*
		 * This is the fetch we've been waiting for.
		 */
		INSIST(devent->fetch == client->query.fetch);
		client->query.fetch = NULL;
		fetch_cancelled = ISC_FALSE;
		/*
		 * Update client->now.
		 */
		isc_stdtime_get(&client->now);
	} else {
		/*
		 * This is a fetch completion event for a cancelled fetch.
		 * Clean up and don't resume the find.
		 */
		fetch_cancelled = ISC_TRUE;
	}
	UNLOCK(&client->query.fetchlock);
	INSIST(client->query.fetch == NULL);

	client->query.attributes &= ~NS_QUERYATTR_RECURSING;
	dns_resolver_destroyfetch(&devent->fetch);

	/*
	 * If this client is shutting down, or this transaction
	 * has timed out, do not resume the find.
	 */
	client_shuttingdown = ns_client_shuttingdown(client);
	if (fetch_cancelled || client_shuttingdown) {
		if (devent->node != NULL)
			dns_db_detachnode(devent->db, &devent->node);
		if (devent->db != NULL)
			dns_db_detach(&devent->db);
		query_putrdataset(client, &devent->rdataset);
		if (devent->sigrdataset != NULL)
			query_putrdataset(client, &devent->sigrdataset);
		isc_event_free(&event);
		if (fetch_cancelled)
			query_error(client, DNS_R_SERVFAIL);
		else
			query_next(client, ISC_R_CANCELED);
		/*
		 * This may destroy the client.
		 */
		ns_client_detach(&client);
	} else {
		query_find(client, devent, 0);
	}
}

static isc_result_t
query_recurse(ns_client_t *client, dns_rdatatype_t qtype, dns_name_t *qdomain,
	      dns_rdataset_t *nameservers)
{
	isc_result_t result;
	dns_rdataset_t *rdataset, *sigrdataset;
	isc_sockaddr_t *peeraddr;

	inc_stats(client, dns_statscounter_recursion);

	/*
	 * We are about to recurse, which means that this client will
	 * be unavailable for serving new requests for an indeterminate
	 * amount of time.  If this client is currently responsible
	 * for handling incoming queries, set up a new client
	 * object to handle them while we are waiting for a
	 * response.  There is no need to replace TCP clients
	 * because those have already been replaced when the
	 * connection was accepted (if allowed by the TCP quota).
	 */
	if (client->recursionquota == NULL) {
		result = isc_quota_attach(&ns_g_server->recursionquota,
					  &client->recursionquota);
		if  (result == ISC_R_SOFTQUOTA) {
			static isc_stdtime_t last = 0;
			isc_stdtime_t now;
			isc_stdtime_get(&now);
			if (now != last) {
				last = now;
				ns_client_log(client, NS_LOGCATEGORY_CLIENT,
					      NS_LOGMODULE_QUERY,
					      ISC_LOG_WARNING,
					      "recursive-clients soft limit "
					      "exceeded, aborting oldest query");
			}
			ns_client_killoldestquery(client);
			result = ISC_R_SUCCESS;
		} else if (result == ISC_R_QUOTA) {
			static isc_stdtime_t last = 0;
			isc_stdtime_t now;
			isc_stdtime_get(&now);
			if (now != last) {
				last = now;
				ns_client_log(client, NS_LOGCATEGORY_CLIENT,
					      NS_LOGMODULE_QUERY,
					      ISC_LOG_WARNING,
					      "no more recursive clients: %s",
					      isc_result_totext(result));
			}
			ns_client_killoldestquery(client);
		}
		if (result == ISC_R_SUCCESS && !client->mortal &&
		    (client->attributes & NS_CLIENTATTR_TCP) == 0) {
			result = ns_client_replace(client);
			if (result != ISC_R_SUCCESS) {
				ns_client_log(client, NS_LOGCATEGORY_CLIENT,
					      NS_LOGMODULE_QUERY,
					      ISC_LOG_WARNING,
					      "ns_client_replace() failed: %s",
					      isc_result_totext(result));
				isc_quota_detach(&client->recursionquota);
			}
		}
		if (result != ISC_R_SUCCESS)
			return (result);
		ns_client_recursing(client);
	}

	/*
	 * Invoke the resolver.
	 */
	REQUIRE(nameservers == NULL || nameservers->type == dns_rdatatype_ns);
	REQUIRE(client->query.fetch == NULL);

	rdataset = query_newrdataset(client);
	if (rdataset == NULL)
		return (ISC_R_NOMEMORY);
	if (WANTDNSSEC(client)) {
		sigrdataset = query_newrdataset(client);
		if (sigrdataset == NULL) {
			query_putrdataset(client, &rdataset);
			return (ISC_R_NOMEMORY);
		}
	} else
		sigrdataset = NULL;

	if (client->query.timerset == ISC_FALSE)
		ns_client_settimeout(client, 60);
	if ((client->attributes & NS_CLIENTATTR_TCP) == 0)
		peeraddr = &client->peeraddr;
	else
		peeraddr = NULL;
	result = dns_resolver_createfetch2(client->view->resolver,
					   client->query.qname,
					   qtype, qdomain, nameservers,
					   NULL, peeraddr, client->message->id,
					   client->query.fetchoptions,
					   client->task,
					   query_resume, client,
					   rdataset, sigrdataset,
					   &client->query.fetch);

	if (result == ISC_R_SUCCESS) {
		/*
		 * Record that we're waiting for an event.  A client which
		 * is shutting down will not be destroyed until all the
		 * events have been received.
		 */
	} else {
		query_putrdataset(client, &rdataset);
		if (sigrdataset != NULL)
			query_putrdataset(client, &sigrdataset);
	}

	return (result);
}

#define MAX_RESTARTS 16

#define QUERY_ERROR(r) \
do { \
	eresult = r; \
	want_restart = ISC_FALSE; \
} while (0)

/*
 * Extract a network address from the RDATA of an A or AAAA
 * record.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOTIMPLEMENTED	The rdata is not a known address type.
 */
static isc_result_t
rdata_tonetaddr(const dns_rdata_t *rdata, isc_netaddr_t *netaddr) {
	struct in_addr ina;
	struct in6_addr in6a;

	switch (rdata->type) {
	case dns_rdatatype_a:
		INSIST(rdata->length == 4);
		memcpy(&ina.s_addr, rdata->data, 4);
		isc_netaddr_fromin(netaddr, &ina);
		return (ISC_R_SUCCESS);
	case dns_rdatatype_aaaa:
		INSIST(rdata->length == 16);
		memcpy(in6a.s6_addr, rdata->data, 16);
		isc_netaddr_fromin6(netaddr, &in6a);
		return (ISC_R_SUCCESS);
	default:
		return (ISC_R_NOTIMPLEMENTED);
	}
}

/*
 * Find the sort order of 'rdata' in the topology-like
 * ACL forming the second element in a 2-element top-level
 * sortlist statement.
 */
static int
query_sortlist_order_2element(const dns_rdata_t *rdata, const void *arg) {
	isc_netaddr_t netaddr;

	if (rdata_tonetaddr(rdata, &netaddr) != ISC_R_SUCCESS)
		return (INT_MAX);
	return (ns_sortlist_addrorder2(&netaddr, arg));
}

/*
 * Find the sort order of 'rdata' in the matching element
 * of a 1-element top-level sortlist statement.
 */
static int
query_sortlist_order_1element(const dns_rdata_t *rdata, const void *arg) {
	isc_netaddr_t netaddr;

	if (rdata_tonetaddr(rdata, &netaddr) != ISC_R_SUCCESS)
		return (INT_MAX);
	return (ns_sortlist_addrorder1(&netaddr, arg));
}

/*
 * Find the sortlist statement that applies to 'client' and set up
 * the sortlist info in in client->message appropriately.
 */
static void
setup_query_sortlist(ns_client_t *client) {
	isc_netaddr_t netaddr;
	dns_rdatasetorderfunc_t order = NULL;
	const void *order_arg = NULL;

	isc_netaddr_fromsockaddr(&netaddr, &client->peeraddr);
	switch (ns_sortlist_setup(client->view->sortlist,
			       &netaddr, &order_arg)) {
	case NS_SORTLISTTYPE_1ELEMENT:
		order = query_sortlist_order_1element;
		break;
	case NS_SORTLISTTYPE_2ELEMENT:
		order = query_sortlist_order_2element;
		break;
	case NS_SORTLISTTYPE_NONE:
		order = NULL;
		break;
	default:
		INSIST(0);
		break;
	}
	dns_message_setsortorder(client->message, order, order_arg);
}

static void
query_addnoqnameproof(ns_client_t *client, dns_rdataset_t *rdataset) {
	isc_buffer_t *dbuf, b;
	dns_name_t *fname;
	dns_rdataset_t *nsec, *nsecsig;
	isc_result_t result = ISC_R_NOMEMORY;

	CTRACE("query_addnoqnameproof");

	fname = NULL;
	nsec = NULL;
	nsecsig = NULL;

	dbuf = query_getnamebuf(client);
	if (dbuf == NULL)
		goto cleanup;
	fname = query_newname(client, dbuf, &b);
	nsec = query_newrdataset(client);
	nsecsig = query_newrdataset(client);
	if (fname == NULL || nsec == NULL || nsecsig == NULL)
		goto cleanup;

	result = dns_rdataset_getnoqname(rdataset, fname, nsec, nsecsig);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	query_addrrset(client, &fname, &nsec, &nsecsig, dbuf,
		       DNS_SECTION_AUTHORITY);

 cleanup:
	if (nsec != NULL)
                query_putrdataset(client, &nsec);
        if (nsecsig != NULL)
                query_putrdataset(client, &nsecsig);
        if (fname != NULL)
                query_releasename(client, &fname);
}

static inline void
answer_in_glue(ns_client_t *client, dns_rdatatype_t qtype) {
	dns_name_t *name;
	dns_message_t *msg;
	dns_section_t section = DNS_SECTION_ADDITIONAL;
	dns_rdataset_t *rdataset = NULL;

	msg = client->message;
	for (name = ISC_LIST_HEAD(msg->sections[section]);
	     name != NULL;
	     name = ISC_LIST_NEXT(name, link))
		if (dns_name_equal(name, client->query.qname)) {
			for (rdataset = ISC_LIST_HEAD(name->list);
			     rdataset != NULL;
			     rdataset = ISC_LIST_NEXT(rdataset, link))
				if (rdataset->type == qtype)
					break;
			break;
		}
	if (rdataset != NULL) {
		ISC_LIST_UNLINK(msg->sections[section], name, link);
		ISC_LIST_PREPEND(msg->sections[section], name, link);
		ISC_LIST_UNLINK(name->list, rdataset, link);
		ISC_LIST_PREPEND(name->list, rdataset, link);
		rdataset->attributes |= DNS_RDATASETATTR_REQUIREDGLUE;
	}
}

#define NS_NAME_INIT(A,B) \
	 { \
		DNS_NAME_MAGIC, \
		A, sizeof(A), sizeof(B), \
		DNS_NAMEATTR_READONLY | DNS_NAMEATTR_ABSOLUTE, \
		B, NULL, { (void *)-1, (void *)-1}, \
		{NULL, NULL} \
	}

static unsigned char inaddr10_offsets[] = { 0, 3, 11, 16 };
static unsigned char inaddr172_offsets[] = { 0, 3, 7, 15, 20 };
static unsigned char inaddr192_offsets[] = { 0, 4, 8, 16, 21 };

static unsigned char inaddr10[] = "\00210\007IN-ADDR\004ARPA";

static unsigned char inaddr16172[] = "\00216\003172\007IN-ADDR\004ARPA";
static unsigned char inaddr17172[] = "\00217\003172\007IN-ADDR\004ARPA";
static unsigned char inaddr18172[] = "\00218\003172\007IN-ADDR\004ARPA";
static unsigned char inaddr19172[] = "\00219\003172\007IN-ADDR\004ARPA";
static unsigned char inaddr20172[] = "\00220\003172\007IN-ADDR\004ARPA";
static unsigned char inaddr21172[] = "\00221\003172\007IN-ADDR\004ARPA";
static unsigned char inaddr22172[] = "\00222\003172\007IN-ADDR\004ARPA";
static unsigned char inaddr23172[] = "\00223\003172\007IN-ADDR\004ARPA";
static unsigned char inaddr24172[] = "\00224\003172\007IN-ADDR\004ARPA";
static unsigned char inaddr25172[] = "\00225\003172\007IN-ADDR\004ARPA";
static unsigned char inaddr26172[] = "\00226\003172\007IN-ADDR\004ARPA";
static unsigned char inaddr27172[] = "\00227\003172\007IN-ADDR\004ARPA";
static unsigned char inaddr28172[] = "\00228\003172\007IN-ADDR\004ARPA";
static unsigned char inaddr29172[] = "\00229\003172\007IN-ADDR\004ARPA";
static unsigned char inaddr30172[] = "\00230\003172\007IN-ADDR\004ARPA";
static unsigned char inaddr31172[] = "\00231\003172\007IN-ADDR\004ARPA";

static unsigned char inaddr168192[] = "\003168\003192\007IN-ADDR\004ARPA";

static dns_name_t rfc1918names[] = {
	NS_NAME_INIT(inaddr10, inaddr10_offsets),
	NS_NAME_INIT(inaddr16172, inaddr172_offsets),
	NS_NAME_INIT(inaddr17172, inaddr172_offsets),
	NS_NAME_INIT(inaddr18172, inaddr172_offsets),
	NS_NAME_INIT(inaddr19172, inaddr172_offsets),
	NS_NAME_INIT(inaddr20172, inaddr172_offsets),
	NS_NAME_INIT(inaddr21172, inaddr172_offsets),
	NS_NAME_INIT(inaddr22172, inaddr172_offsets),
	NS_NAME_INIT(inaddr23172, inaddr172_offsets),
	NS_NAME_INIT(inaddr24172, inaddr172_offsets),
	NS_NAME_INIT(inaddr25172, inaddr172_offsets),
	NS_NAME_INIT(inaddr26172, inaddr172_offsets),
	NS_NAME_INIT(inaddr27172, inaddr172_offsets),
	NS_NAME_INIT(inaddr28172, inaddr172_offsets),
	NS_NAME_INIT(inaddr29172, inaddr172_offsets),
	NS_NAME_INIT(inaddr30172, inaddr172_offsets),
	NS_NAME_INIT(inaddr31172, inaddr172_offsets),
	NS_NAME_INIT(inaddr168192, inaddr192_offsets)
};


static unsigned char prisoner_data[] = "\010prisoner\004iana\003org";
static unsigned char hostmaster_data[] = "\012hostmaster\014root-servers\003org";

static unsigned char prisoner_offsets[] = { 0, 9, 14, 18 };
static unsigned char hostmaster_offsets[] = { 0, 11, 24, 28 };

static dns_name_t prisoner = NS_NAME_INIT(prisoner_data, prisoner_offsets);
static dns_name_t hostmaster = NS_NAME_INIT(hostmaster_data, hostmaster_offsets);

static void
warn_rfc1918(ns_client_t *client, dns_name_t *fname, dns_rdataset_t *rdataset) {
	unsigned int i;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_soa_t soa;
	dns_rdataset_t found;
	isc_result_t result;
	
	for (i = 0; i < (sizeof(rfc1918names)/sizeof(*rfc1918names)); i++) {
		if (dns_name_issubdomain(fname, &rfc1918names[i])) {
			dns_rdataset_init(&found);
			result = dns_ncache_getrdataset(rdataset,
						        &rfc1918names[i],
							dns_rdatatype_soa,
							&found);
			if (result != ISC_R_SUCCESS)
				return;

			result = dns_rdataset_first(&found);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
			dns_rdataset_current(&found, &rdata);
			result = dns_rdata_tostruct(&rdata, &soa, NULL);
			if (result != ISC_R_SUCCESS)
				return;
			if (dns_name_equal(&soa.origin, &prisoner) &&
			    dns_name_equal(&soa.contact, &hostmaster)) {
				char buf[DNS_NAME_FORMATSIZE];
				dns_name_format(fname, buf, sizeof(buf));
				ns_client_log(client, DNS_LOGCATEGORY_SECURITY,
					      NS_LOGMODULE_QUERY,
					      ISC_LOG_WARNING,
					      "RFC 1918 response from "
					      "Internet for %s", buf);
			}
			dns_rdataset_disassociate(&found);
			return;
		}
	}
}

/*
 * Do the bulk of query processing for the current query of 'client'.
 * If 'event' is non-NULL, we are returning from recursion and 'qtype'
 * is ignored.  Otherwise, 'qtype' is the query type.
 */
static void
query_find(ns_client_t *client, dns_fetchevent_t *event, dns_rdatatype_t qtype)
{
	dns_db_t *db, *zdb;
	dns_dbnode_t *node;
	dns_rdatatype_t type;
	dns_name_t *fname, *zfname, *tname, *prefix;
	dns_rdataset_t *rdataset, *trdataset;
	dns_rdataset_t *sigrdataset, *zrdataset, *zsigrdataset;
	dns_rdataset_t **sigrdatasetp;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdatasetiter_t *rdsiter;
	isc_boolean_t want_restart, authoritative, is_zone, need_wildcardproof;
	unsigned int n, nlabels;
	dns_namereln_t namereln;
	int order;
	isc_buffer_t *dbuf;
	isc_buffer_t b;
	isc_result_t result, eresult;
	dns_fixedname_t fixed;
	dns_fixedname_t wildcardname;
	dns_dbversion_t *version;
	dns_zone_t *zone;
	dns_rdata_cname_t cname;
	dns_rdata_dname_t dname;
	unsigned int options;
	isc_boolean_t empty_wild;
	dns_rdataset_t *noqname;
	dns_rdataset_t tmprdataset;
	unsigned int dboptions;

	CTRACE("query_find");

	/*
	 * One-time initialization.
	 *
	 * It's especially important to initialize anything that the cleanup
	 * code might cleanup.
	 */

	eresult = ISC_R_SUCCESS;
	fname = NULL;
	zfname = NULL;
	rdataset = NULL;
	zrdataset = NULL;
	sigrdataset = NULL;
	zsigrdataset = NULL;
	node = NULL;
	db = NULL;
	zdb = NULL;
	version = NULL;
	zone = NULL;
	need_wildcardproof = ISC_FALSE;
	empty_wild = ISC_FALSE;
	options = 0;

	if (event != NULL) {
		/*
		 * We're returning from recursion.  Restore the query context
		 * and resume.
		 */

		want_restart = ISC_FALSE;
		authoritative = ISC_FALSE;
		is_zone = ISC_FALSE;

		qtype = event->qtype;
		if (qtype == dns_rdatatype_rrsig || qtype == dns_rdatatype_sig)
			type = dns_rdatatype_any;
		else
			type = qtype;
		db = event->db;
		node = event->node;
		rdataset = event->rdataset;
		sigrdataset = event->sigrdataset;

		/*
		 * We'll need some resources...
		 */
		dbuf = query_getnamebuf(client);
		if (dbuf == NULL) {
			QUERY_ERROR(DNS_R_SERVFAIL);
			goto cleanup;
		}
		fname = query_newname(client, dbuf, &b);
		if (fname == NULL) {
			QUERY_ERROR(DNS_R_SERVFAIL);
			goto cleanup;
		}
		tname = dns_fixedname_name(&event->foundname);
		result = dns_name_copy(tname, fname, NULL);
		if (result != ISC_R_SUCCESS) {
			QUERY_ERROR(DNS_R_SERVFAIL);
			goto cleanup;
		}

		result = event->result;

		goto resume;
	}

	/*
	 * Not returning from recursion.
	 */

	/*
	 * If it's a SIG query, we'll iterate the node.
	 */
	if (qtype == dns_rdatatype_rrsig || qtype == dns_rdatatype_sig)
		type = dns_rdatatype_any;
	else
		type = qtype;

 restart:
	CTRACE("query_find: restart");
	want_restart = ISC_FALSE;
	authoritative = ISC_FALSE;
	version = NULL;
	need_wildcardproof = ISC_FALSE;

	if (client->view->checknames &&
	    !dns_rdata_checkowner(client->query.qname,
				  client->message->rdclass,
				  qtype, ISC_FALSE)) {
		char namebuf[DNS_NAME_FORMATSIZE];
		char typename[DNS_RDATATYPE_FORMATSIZE];
		char classname[DNS_RDATACLASS_FORMATSIZE];

		dns_name_format(client->query.qname, namebuf, sizeof(namebuf));
		dns_rdatatype_format(qtype, typename, sizeof(typename));
		dns_rdataclass_format(client->message->rdclass, classname,
				      sizeof(classname));
		ns_client_log(client, DNS_LOGCATEGORY_SECURITY,
			      NS_LOGMODULE_QUERY, ISC_LOG_ERROR,
			      "check-names failure %s/%s/%s", namebuf,
			      typename, classname);
		QUERY_ERROR(DNS_R_REFUSED);
		goto cleanup;
	}

	/*
	 * First we must find the right database.
	 */
	options &= DNS_GETDB_NOLOG; /* Preserve DNS_GETDB_NOLOG. */
	if (dns_rdatatype_atparent(qtype) &&
	    !dns_name_equal(client->query.qname, dns_rootname))
		options |= DNS_GETDB_NOEXACT;
	result = query_getdb(client, client->query.qname, qtype, options,
			     &zone, &db, &version, &is_zone);
	if ((result != ISC_R_SUCCESS || !is_zone) && !RECURSIONOK(client) &&
	    (options & DNS_GETDB_NOEXACT) != 0 && qtype == dns_rdatatype_ds) {
		/*
		 * Look to see if we are authoritative for the
		 * child zone if the query type is DS.
		 */
		dns_db_t *tdb = NULL;
		dns_zone_t *tzone = NULL;
		dns_dbversion_t *tversion = NULL;
		isc_result_t tresult;

		tresult = query_getzonedb(client, client->query.qname, qtype,
					 DNS_GETDB_PARTIAL, &tzone, &tdb,
					 &tversion);
		if (tresult == ISC_R_SUCCESS) {
			options &= ~DNS_GETDB_NOEXACT;
			query_putrdataset(client, &rdataset);
			if (db != NULL)
				dns_db_detach(&db);
			if (zone != NULL)
				dns_zone_detach(&zone);
			version = tversion;
			db = tdb;
			zone = tzone;
			is_zone = ISC_TRUE;
			result = ISC_R_SUCCESS;
		} else {
			if (tdb != NULL)
				dns_db_detach(&tdb);
			if (tzone != NULL)
				dns_zone_detach(&tzone);
		}
	}
	if (result != ISC_R_SUCCESS) {
		if (result == DNS_R_REFUSED) {
			if (!PARTIALANSWER(client))
				QUERY_ERROR(DNS_R_REFUSED);
		} else
			QUERY_ERROR(DNS_R_SERVFAIL);
		goto cleanup;
	}

	if (is_zone)
		authoritative = ISC_TRUE;

	if (event == NULL && client->query.restarts == 0) {
		if (is_zone) {
#ifdef DLZ
			if (zone != NULL) {
				/*
				 * if is_zone = true, zone = NULL then this is
				 * a DLZ zone.  Don't attempt to attach zone.
				 */
#endif
				dns_zone_attach(zone, &client->query.authzone);
#ifdef DLZ
			}
#endif
			dns_db_attach(db, &client->query.authdb);
		}
		client->query.authdbset = ISC_TRUE;
	}

 db_find:
	CTRACE("query_find: db_find");
	/*
	 * We'll need some resources...
	 */
	dbuf = query_getnamebuf(client);
	if (dbuf == NULL) {
		QUERY_ERROR(DNS_R_SERVFAIL);
		goto cleanup;
	}
	fname = query_newname(client, dbuf, &b);
	rdataset = query_newrdataset(client);
	if (fname == NULL || rdataset == NULL) {
		QUERY_ERROR(DNS_R_SERVFAIL);
		goto cleanup;
	}
	if (WANTDNSSEC(client)) {
		sigrdataset = query_newrdataset(client);
		if (sigrdataset == NULL) {
			QUERY_ERROR(DNS_R_SERVFAIL);
			goto cleanup;
		}
	}

	/*
	 * Now look for an answer in the database.
	 */
	dboptions = client->query.dboptions;
	if (sigrdataset == NULL && client->view->enablednssec) {
		/*
		 * If the client doesn't want DNSSEC we still want to
		 * look for any data pending validation to save a remote
		 * lookup if possible.
		 */
		dns_rdataset_init(&tmprdataset);
		sigrdataset = &tmprdataset;
		dboptions |= DNS_DBFIND_PENDINGOK;
	}
 refind:
	result = dns_db_find(db, client->query.qname, version, type,
			     dboptions, client->now, &node, fname,
			     rdataset, sigrdataset);
	/*
	 * If we have found pending data try to validate it.
	 * If the data does not validate as secure and we can't
	 * use the unvalidated data requery the database with
	 * pending disabled to prevent infinite looping.
	 */
	if (result != ISC_R_SUCCESS || !DNS_TRUST_PENDING(rdataset->trust))
		goto validation_done;
	if (validate(client, db, fname, rdataset, sigrdataset))
		goto validation_done;
	if (rdataset->trust != dns_trust_pending_answer ||
	    !PENDINGOK(client->query.dboptions)) {
		dns_rdataset_disassociate(rdataset);
		if (sigrdataset != NULL &&
		    dns_rdataset_isassociated(sigrdataset))
			dns_rdataset_disassociate(sigrdataset);
		if (sigrdataset == &tmprdataset)
			sigrdataset = NULL;
		dns_db_detachnode(db, &node);
		dboptions &= ~DNS_DBFIND_PENDINGOK;
		goto refind;
	}
 validation_done:
	if (sigrdataset == &tmprdataset) {
		if (dns_rdataset_isassociated(sigrdataset))
			dns_rdataset_disassociate(sigrdataset);
		sigrdataset = NULL;
	}

 resume:
	CTRACE("query_find: resume");
	switch (result) {
	case ISC_R_SUCCESS:
		/*
		 * This case is handled in the main line below.
		 */
		break;
	case DNS_R_GLUE:
	case DNS_R_ZONECUT:
		/*
		 * These cases are handled in the main line below.
		 */
		INSIST(is_zone);
		authoritative = ISC_FALSE;
		break;
	case ISC_R_NOTFOUND:
		/*
		 * The cache doesn't even have the root NS.  Get them from
		 * the hints DB.
		 */
		INSIST(!is_zone);
		if (db != NULL)
			dns_db_detach(&db);

		if (client->view->hints == NULL) {
			/* We have no hints. */
			result = ISC_R_FAILURE;
		} else {
			dns_db_attach(client->view->hints, &db);
			result = dns_db_find(db, dns_rootname,
					     NULL, dns_rdatatype_ns,
					     0, client->now, &node, fname,
					     rdataset, sigrdataset);
		}
		if (result != ISC_R_SUCCESS) {
			/*
			 * Nonsensical root hints may require cleanup.
			 */
			if (dns_rdataset_isassociated(rdataset))
				dns_rdataset_disassociate(rdataset);
			if (sigrdataset != NULL &&
			    dns_rdataset_isassociated(sigrdataset))
				dns_rdataset_disassociate(sigrdataset);
			if (node != NULL)
				dns_db_detachnode(db, &node);

			/*
			 * We don't have any root server hints, but
			 * we may have working forwarders, so try to
			 * recurse anyway.
			 */
			if (RECURSIONOK(client)) {
				result = query_recurse(client, qtype,
						       NULL, NULL);
				if (result == ISC_R_SUCCESS)
					client->query.attributes |=
						NS_QUERYATTR_RECURSING;
				else if (result == DNS_R_DUPLICATE ||
					 result == DNS_R_DROP) {
					/* Duplicate query. */
					QUERY_ERROR(result);
				} else {
					/* Unable to recurse. */
					QUERY_ERROR(DNS_R_SERVFAIL);
				}
				goto cleanup;
			} else {
				/* Unable to give root server referral. */
				QUERY_ERROR(DNS_R_SERVFAIL);
				goto cleanup;
			}
		}
		/*
		 * XXXRTH  We should trigger root server priming here.
		 */
		/* FALLTHROUGH */
	case DNS_R_DELEGATION:
		authoritative = ISC_FALSE;
		if (is_zone) {
			/*
			 * Look to see if we are authoritative for the
			 * child zone if the query type is DS.
			 */
			if (!RECURSIONOK(client) &&
			    (options & DNS_GETDB_NOEXACT) != 0 &&
			    qtype == dns_rdatatype_ds) {
				dns_db_t *tdb = NULL;
				dns_zone_t *tzone = NULL;
				dns_dbversion_t *tversion = NULL;
				result = query_getzonedb(client,
							 client->query.qname,
							 qtype,
							 DNS_GETDB_PARTIAL,
							 &tzone, &tdb,
							 &tversion);
				if (result == ISC_R_SUCCESS) {
					options &= ~DNS_GETDB_NOEXACT;
					query_putrdataset(client, &rdataset);
					if (sigrdataset != NULL)
						query_putrdataset(client,
								  &sigrdataset);
					if (fname != NULL)
						query_releasename(client,
								  &fname);
					if (node != NULL)
						dns_db_detachnode(db, &node);
					if (db != NULL)
						dns_db_detach(&db);
					if (zone != NULL)
						dns_zone_detach(&zone);
					version = tversion;
					db = tdb;
					zone = tzone;
					authoritative = ISC_TRUE;
					goto db_find;
				}
				if (tdb != NULL)
					dns_db_detach(&tdb);
				if (tzone != NULL)
					dns_zone_detach(&tzone);
			}
			/*
			 * We're authoritative for an ancestor of QNAME.
			 */
			if (!USECACHE(client) || !RECURSIONOK(client)) {
				/*
				 * If we don't have a cache, this is the best
				 * answer.
				 *
				 * If the client is making a nonrecursive
				 * query we always give out the authoritative
				 * delegation.  This way even if we get
				 * junk in our cache, we won't fail in our
				 * role as the delegating authority if another
				 * nameserver asks us about a delegated
				 * subzone.
				 *
				 * We enable the retrieval of glue for this
				 * database by setting client->query.gluedb.
				 */
				client->query.gluedb = db;
				client->query.isreferral = ISC_TRUE;
				/*
				 * We must ensure NOADDITIONAL is off,
				 * because the generation of
				 * additional data is required in
				 * delegations.
				 */
				client->query.attributes &=
					~NS_QUERYATTR_NOADDITIONAL;
				if (sigrdataset != NULL)
					sigrdatasetp = &sigrdataset;
				else
					sigrdatasetp = NULL;
				query_addrrset(client, &fname,
					       &rdataset, sigrdatasetp,
					       dbuf, DNS_SECTION_AUTHORITY);
				client->query.gluedb = NULL;
				if (WANTDNSSEC(client) && dns_db_issecure(db))
					query_addds(client, db, node, version);
			} else {
				/*
				 * We might have a better answer or delegation
				 * in the cache.  We'll remember the current
				 * values of fname, rdataset, and sigrdataset.
				 * We'll then go looking for QNAME in the
				 * cache.  If we find something better, we'll
				 * use it instead.
				 */
				query_keepname(client, fname, dbuf);
				zdb = db;
				zfname = fname;
				fname = NULL;
				zrdataset = rdataset;
				rdataset = NULL;
				zsigrdataset = sigrdataset;
				sigrdataset = NULL;
				dns_db_detachnode(db, &node);
				version = NULL;
				db = NULL;
				dns_db_attach(client->view->cachedb, &db);
				is_zone = ISC_FALSE;
				goto db_find;
			}
		} else {
			if (zfname != NULL &&
			    !dns_name_issubdomain(fname, zfname)) {
				/*
				 * We've already got a delegation from
				 * authoritative data, and it is better
				 * than what we found in the cache.  Use
				 * it instead of the cache delegation.
				 */
				query_releasename(client, &fname);
				fname = zfname;
				zfname = NULL;
				/*
				 * We've already done query_keepname() on
				 * zfname, so we must set dbuf to NULL to
				 * prevent query_addrrset() from trying to
				 * call query_keepname() again.
				 */
				dbuf = NULL;
				query_putrdataset(client, &rdataset);
				if (sigrdataset != NULL)
					query_putrdataset(client,
							  &sigrdataset);
				rdataset = zrdataset;
				zrdataset = NULL;
				sigrdataset = zsigrdataset;
				zsigrdataset = NULL;
				/*
				 * We don't clean up zdb here because we
				 * may still need it.  It will get cleaned
				 * up by the main cleanup code.
				 */
			}

			if (RECURSIONOK(client)) {
				/*
				 * Recurse!
				 */
				if (dns_rdatatype_atparent(type))
					result = query_recurse(client, qtype,
							       NULL, NULL);
				else
					result = query_recurse(client, qtype,
							       fname, rdataset);
				if (result == ISC_R_SUCCESS)
					client->query.attributes |=
						NS_QUERYATTR_RECURSING;
				else if (result == DNS_R_DUPLICATE ||
					 result == DNS_R_DROP)
					QUERY_ERROR(result);
				else
					QUERY_ERROR(DNS_R_SERVFAIL);
			} else {
				/*
				 * This is the best answer.
				 */
				client->query.attributes |=
					NS_QUERYATTR_CACHEGLUEOK;
				client->query.gluedb = zdb;
				client->query.isreferral = ISC_TRUE;
				/*
				 * We must ensure NOADDITIONAL is off,
				 * because the generation of
				 * additional data is required in
				 * delegations.
				 */
				client->query.attributes &=
					~NS_QUERYATTR_NOADDITIONAL;
				if (sigrdataset != NULL)
					sigrdatasetp = &sigrdataset;
				else
					sigrdatasetp = NULL;
				query_addrrset(client, &fname,
					       &rdataset, sigrdatasetp,
					       dbuf, DNS_SECTION_AUTHORITY);
				client->query.gluedb = NULL;
				client->query.attributes &=
					~NS_QUERYATTR_CACHEGLUEOK;
				if (WANTDNSSEC(client))
					query_addds(client, db, node, version);
			}
		}
		goto cleanup;
	case DNS_R_EMPTYNAME:
		result = DNS_R_NXRRSET;
		/* FALLTHROUGH */
	case DNS_R_NXRRSET:
		INSIST(is_zone);
		if (dns_rdataset_isassociated(rdataset)) {
			/*
			 * If we've got a NSEC record, we need to save the
			 * name now because we're going call query_addsoa()
			 * below, and it needs to use the name buffer.
			 */
			query_keepname(client, fname, dbuf);
		} else {
			/*
			 * We're not going to use fname, and need to release
			 * our hold on the name buffer so query_addsoa()
			 * may use it.
			 */
			query_releasename(client, &fname);
		}
		/*
		 * Add SOA.
		 */
		result = query_addsoa(client, db, version, ISC_FALSE);
		if (result != ISC_R_SUCCESS) {
			QUERY_ERROR(result);
			goto cleanup;
		}
		/*
		 * Add NSEC record if we found one.
		 */
		if (WANTDNSSEC(client)) {
			if (dns_rdataset_isassociated(rdataset))
				query_addnxrrsetnsec(client, db, version,
						     &fname, &rdataset,
						     &sigrdataset);
		}
		goto cleanup;
	case DNS_R_EMPTYWILD:
		empty_wild = ISC_TRUE;
		/* FALLTHROUGH */
	case DNS_R_NXDOMAIN:
		INSIST(is_zone);
		if (dns_rdataset_isassociated(rdataset)) {
			/*
			 * If we've got a NSEC record, we need to save the
			 * name now because we're going call query_addsoa()
			 * below, and it needs to use the name buffer.
			 */
			query_keepname(client, fname, dbuf);
		} else {
			/*
			 * We're not going to use fname, and need to release
			 * our hold on the name buffer so query_addsoa()
			 * may use it.
			 */
			query_releasename(client, &fname);
		}
		/*
		 * Add SOA.  If the query was for a SOA record force the
		 * ttl to zero so that it is possible for clients to find
		 * the containing zone of an arbitrary name with a stub
		 * resolver and not have it cached.
		 */
		if (qtype == dns_rdatatype_soa &&
#ifdef DLZ
		    zone != NULL &&
#endif
		    dns_zone_getzeronosoattl(zone))
			result = query_addsoa(client, db, version, ISC_TRUE);
		else
			result = query_addsoa(client, db, version, ISC_FALSE);
		if (result != ISC_R_SUCCESS) {
			QUERY_ERROR(result);
			goto cleanup;
		}
		/*
		 * Add NSEC record if we found one.
		 */
		if (dns_rdataset_isassociated(rdataset)) {
			if (WANTDNSSEC(client)) {
				query_addrrset(client, &fname, &rdataset,
					       &sigrdataset,
					       NULL, DNS_SECTION_AUTHORITY);
				query_addwildcardproof(client, db, version,
						       client->query.qname,
						       ISC_FALSE);
			}
		}
		/*
		 * Set message rcode.
		 */
		if (empty_wild)
			client->message->rcode = dns_rcode_noerror;
		else
			client->message->rcode = dns_rcode_nxdomain;
		goto cleanup;
	case DNS_R_NCACHENXDOMAIN:
	case DNS_R_NCACHENXRRSET:
		INSIST(!is_zone);
		authoritative = ISC_FALSE;
		/*
		 * Set message rcode, if required.
		 */
		if (result == DNS_R_NCACHENXDOMAIN)
			client->message->rcode = dns_rcode_nxdomain;
		/*
		 * Look for RFC 1918 leakage from Internet.
		 */
		if (result == DNS_R_NCACHENXDOMAIN &&
		    qtype == dns_rdatatype_ptr &&
		    client->message->rdclass == dns_rdataclass_in &&
		    dns_name_countlabels(fname) == 7)
			warn_rfc1918(client, fname, rdataset);
		/*
		 * We don't call query_addrrset() because we don't need any
		 * of its extra features (and things would probably break!).
		 */
		query_keepname(client, fname, dbuf);
		dns_message_addname(client->message, fname,
				    DNS_SECTION_AUTHORITY);
		ISC_LIST_APPEND(fname->list, rdataset, link);
		fname = NULL;
		rdataset = NULL;
		goto cleanup;
	case DNS_R_CNAME:
		/*
		 * Keep a copy of the rdataset.  We have to do this because
		 * query_addrrset may clear 'rdataset' (to prevent the
		 * cleanup code from cleaning it up).
		 */
		trdataset = rdataset;
		/*
		 * Add the CNAME to the answer section.
		 */
		if (sigrdataset != NULL)
			sigrdatasetp = &sigrdataset;
		else
			sigrdatasetp = NULL;
		if (WANTDNSSEC(client) &&
		    (fname->attributes & DNS_NAMEATTR_WILDCARD) != 0)
		{
			dns_fixedname_init(&wildcardname);
			dns_name_copy(fname, dns_fixedname_name(&wildcardname),
				      NULL);
			need_wildcardproof = ISC_TRUE;
		}
		if ((rdataset->attributes & DNS_RDATASETATTR_NOQNAME) != 0 &&
		     WANTDNSSEC(client))
			noqname = rdataset;
		else
			noqname = NULL;
		query_addrrset(client, &fname, &rdataset, sigrdatasetp, dbuf,
			       DNS_SECTION_ANSWER);
		if (noqname != NULL)
			query_addnoqnameproof(client, noqname);
		/*
		 * We set the PARTIALANSWER attribute so that if anything goes
		 * wrong later on, we'll return what we've got so far.
		 */
		client->query.attributes |= NS_QUERYATTR_PARTIALANSWER;
		/*
		 * Reset qname to be the target name of the CNAME and restart
		 * the query.
		 */
		tname = NULL;
		result = dns_message_gettempname(client->message, &tname);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		result = dns_rdataset_first(trdataset);
		if (result != ISC_R_SUCCESS) {
			dns_message_puttempname(client->message, &tname);
			goto cleanup;
		}
		dns_rdataset_current(trdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &cname, NULL);
		dns_rdata_reset(&rdata);
		if (result != ISC_R_SUCCESS) {
			dns_message_puttempname(client->message, &tname);
			goto cleanup;
		}
		dns_name_init(tname, NULL);
		result = dns_name_dup(&cname.cname, client->mctx, tname);
		if (result != ISC_R_SUCCESS) {
			dns_message_puttempname(client->message, &tname);
			dns_rdata_freestruct(&cname);
			goto cleanup;
		}
		dns_rdata_freestruct(&cname);
		ns_client_qnamereplace(client, tname);
		want_restart = ISC_TRUE;
		if (!WANTRECURSION(client))
			options |= DNS_GETDB_NOLOG;
		goto addauth;
	case DNS_R_DNAME:
		/*
		 * Compare the current qname to the found name.  We need
		 * to know how many labels and bits are in common because
		 * we're going to have to split qname later on.
		 */
		namereln = dns_name_fullcompare(client->query.qname, fname,
						&order, &nlabels);
		INSIST(namereln == dns_namereln_subdomain);
		/*
		 * Keep a copy of the rdataset.  We have to do this because
		 * query_addrrset may clear 'rdataset' (to prevent the
		 * cleanup code from cleaning it up).
		 */
		trdataset = rdataset;
		/*
		 * Add the DNAME to the answer section.
		 */
		if (sigrdataset != NULL)
			sigrdatasetp = &sigrdataset;
		else
			sigrdatasetp = NULL;
		if (WANTDNSSEC(client) &&
		    (fname->attributes & DNS_NAMEATTR_WILDCARD) != 0)
		{
			dns_fixedname_init(&wildcardname);
			dns_name_copy(fname, dns_fixedname_name(&wildcardname),
				      NULL);
			need_wildcardproof = ISC_TRUE;
		}
		query_addrrset(client, &fname, &rdataset, sigrdatasetp, dbuf,
			       DNS_SECTION_ANSWER);
		/*
		 * We set the PARTIALANSWER attribute so that if anything goes
		 * wrong later on, we'll return what we've got so far.
		 */
		client->query.attributes |= NS_QUERYATTR_PARTIALANSWER;
		/*
		 * Get the target name of the DNAME.
		 */
		tname = NULL;
		result = dns_message_gettempname(client->message, &tname);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		result = dns_rdataset_first(trdataset);
		if (result != ISC_R_SUCCESS) {
			dns_message_puttempname(client->message, &tname);
			goto cleanup;
		}
		dns_rdataset_current(trdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &dname, NULL);
		dns_rdata_reset(&rdata);
		if (result != ISC_R_SUCCESS) {
			dns_message_puttempname(client->message, &tname);
			goto cleanup;
		}
		dns_name_init(tname, NULL);
		dns_name_clone(&dname.dname, tname);
		dns_rdata_freestruct(&dname);
		/*
		 * Construct the new qname.
		 */
		dns_fixedname_init(&fixed);
		prefix = dns_fixedname_name(&fixed);
		dns_name_split(client->query.qname, nlabels, prefix, NULL);
		INSIST(fname == NULL);
		dbuf = query_getnamebuf(client);
		if (dbuf == NULL) {
			dns_message_puttempname(client->message, &tname);
			goto cleanup;
		}
		fname = query_newname(client, dbuf, &b);
		if (fname == NULL) {
			dns_message_puttempname(client->message, &tname);
			goto cleanup;
		}
		result = dns_name_concatenate(prefix, tname, fname, NULL);
		if (result != ISC_R_SUCCESS) {
			dns_message_puttempname(client->message, &tname);
			if (result == ISC_R_NOSPACE) {
				/*
				 * RFC2672, section 4.1, subsection 3c says
				 * we should return YXDOMAIN if the constructed
				 * name would be too long.
				 */
				client->message->rcode = dns_rcode_yxdomain;
			}
			goto cleanup;
		}
		query_keepname(client, fname, dbuf);
		/*
		 * Synthesize a CNAME for this DNAME.
		 *
		 * We want to synthesize a CNAME since if we don't
		 * then older software that doesn't understand DNAME
		 * will not chain like it should.
		 *
		 * We do not try to synthesize a signature because we hope
		 * that security aware servers will understand DNAME.  Also,
		 * even if we had an online key, making a signature
		 * on-the-fly is costly, and not really legitimate anyway
		 * since the synthesized CNAME is NOT in the zone.
		 */
		dns_name_init(tname, NULL);
		(void)query_addcnamelike(client, client->query.qname, fname,
					 trdataset->trust, &tname,
					 dns_rdatatype_cname);
		if (tname != NULL)
			dns_message_puttempname(client->message, &tname);
		/*
		 * Switch to the new qname and restart.
		 */
		ns_client_qnamereplace(client, fname);
		fname = NULL;
		want_restart = ISC_TRUE;
		if (!WANTRECURSION(client))
			options |= DNS_GETDB_NOLOG;
		goto addauth;
	default:
		/*
		 * Something has gone wrong.
		 */
		QUERY_ERROR(DNS_R_SERVFAIL);
		goto cleanup;
	}

	if (WANTDNSSEC(client) &&
	    (fname->attributes & DNS_NAMEATTR_WILDCARD) != 0)
	{
		dns_fixedname_init(&wildcardname);
		dns_name_copy(fname, dns_fixedname_name(&wildcardname), NULL);
		need_wildcardproof = ISC_TRUE;
	}

	if (type == dns_rdatatype_any) {
		/*
		 * XXXRTH  Need to handle zonecuts with special case
		 * code.
		 */
		n = 0;
		rdsiter = NULL;
		result = dns_db_allrdatasets(db, node, version, 0, &rdsiter);
		if (result != ISC_R_SUCCESS) {
			QUERY_ERROR(DNS_R_SERVFAIL);
			goto cleanup;
		}
		/*
		 * Calling query_addrrset() with a non-NULL dbuf is going
		 * to either keep or release the name.  We don't want it to
		 * release fname, since we may have to call query_addrrset()
		 * more than once.  That means we have to call query_keepname()
		 * now, and pass a NULL dbuf to query_addrrset().
		 *
		 * If we do a query_addrrset() below, we must set fname to
		 * NULL before leaving this block, otherwise we might try to
		 * cleanup fname even though we're using it!
		 */
		query_keepname(client, fname, dbuf);
		tname = fname;
		result = dns_rdatasetiter_first(rdsiter);
		while (result == ISC_R_SUCCESS) {
			dns_rdatasetiter_current(rdsiter, rdataset);
			if ((qtype == dns_rdatatype_any ||
			     rdataset->type == qtype) && rdataset->type != 0) {
				query_addrrset(client,
					       fname != NULL ? &fname : &tname,
					       &rdataset, NULL,
					       NULL, DNS_SECTION_ANSWER);
				n++;
				INSIST(tname != NULL);
				/*
				 * rdataset is non-NULL only in certain pathological
				 * cases involving DNAMEs.
				 */
				if (rdataset != NULL)
					query_putrdataset(client, &rdataset);
				rdataset = query_newrdataset(client);
				if (rdataset == NULL)
					break;
			} else {
				/*
				 * We're not interested in this rdataset.
				 */
				dns_rdataset_disassociate(rdataset);
			}
			result = dns_rdatasetiter_next(rdsiter);
		}

		if (fname != NULL)
			dns_message_puttempname(client->message, &fname);

		if (n == 0) {
			/*
			 * We didn't match any rdatasets.
			 */
			if (qtype == dns_rdatatype_rrsig &&
			    result == ISC_R_NOMORE) {
				/*
				 * XXXRTH  If this is a secure zone and we
				 * didn't find any SIGs, we should generate
				 * an error unless we were searching for
				 * glue.  Ugh.
				 */
				if (!is_zone) {
					authoritative = ISC_FALSE;
					dns_rdatasetiter_destroy(&rdsiter);
					if (RECURSIONOK(client)) {
						result = query_recurse(client,
								       qtype,
								       NULL,
								       NULL);
						if (result == ISC_R_SUCCESS)
						    client->query.attributes |=
							NS_QUERYATTR_RECURSING;
						else
						    QUERY_ERROR(DNS_R_SERVFAIL);					}
					goto addauth;
				}
				/*
				 * We were searching for SIG records in
				 * a nonsecure zone.  Send a "no error,
				 * no data" response.
				 */
				/*
				 * Add SOA.
				 */
				result = query_addsoa(client, db, version,
						      ISC_FALSE);
				if (result == ISC_R_SUCCESS)
					result = ISC_R_NOMORE;
			} else {
				/*
				 * Something went wrong.
				 */
				result = DNS_R_SERVFAIL;
			}
		}
		dns_rdatasetiter_destroy(&rdsiter);
		if (result != ISC_R_NOMORE) {
			QUERY_ERROR(DNS_R_SERVFAIL);
			goto cleanup;
		}
	} else {
		/*
		 * This is the "normal" case -- an ordinary question to which
		 * we know the answer.
		 */
		if (sigrdataset != NULL)
			sigrdatasetp = &sigrdataset;
		else
			sigrdatasetp = NULL;
		if ((rdataset->attributes & DNS_RDATASETATTR_NOQNAME) != 0 &&
		     WANTDNSSEC(client))
			noqname = rdataset;
		else
			noqname = NULL;
		/*
		 * BIND 8 priming queries need the additional section.
		 */
		if (is_zone && qtype == dns_rdatatype_ns &&
		    dns_name_equal(client->query.qname, dns_rootname))
			client->query.attributes &= ~NS_QUERYATTR_NOADDITIONAL;

		query_addrrset(client, &fname, &rdataset, sigrdatasetp, dbuf,
			       DNS_SECTION_ANSWER);
		if (noqname != NULL)
			query_addnoqnameproof(client, noqname);
		/*
		 * We shouldn't ever fail to add 'rdataset'
		 * because it's already in the answer.
		 */
		INSIST(rdataset == NULL);
	}

 addauth:
	CTRACE("query_find: addauth");
	/*
	 * Add NS records to the authority section (if we haven't already
	 * added them to the answer section).
	 */
	if (!want_restart && !NOAUTHORITY(client)) {
		if (is_zone) {
			if (!((qtype == dns_rdatatype_ns ||
			       qtype == dns_rdatatype_any) &&
			      dns_name_equal(client->query.qname,
					     dns_db_origin(db))))
				(void)query_addns(client, db, version);
		} else if (qtype != dns_rdatatype_ns) {
			if (fname != NULL)
				query_releasename(client, &fname);
			query_addbestns(client);
		}
	}

	/*
	 * Add NSEC records to the authority section if they're needed for
	 * DNSSEC wildcard proofs.
	 */
	if (need_wildcardproof && dns_db_issecure(db))
		query_addwildcardproof(client, db, version,
				       dns_fixedname_name(&wildcardname),
				       ISC_TRUE);
 cleanup:
	CTRACE("query_find: cleanup");
	/*
	 * General cleanup.
	 */
	if (rdataset != NULL)
		query_putrdataset(client, &rdataset);
	if (sigrdataset != NULL)
		query_putrdataset(client, &sigrdataset);
	if (fname != NULL)
		query_releasename(client, &fname);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (db != NULL)
		dns_db_detach(&db);
	if (zone != NULL)
		dns_zone_detach(&zone);
	if (zdb != NULL) {
		query_putrdataset(client, &zrdataset);
		if (zsigrdataset != NULL)
			query_putrdataset(client, &zsigrdataset);
		if (zfname != NULL)
			query_releasename(client, &zfname);
		dns_db_detach(&zdb);
	}
	if (event != NULL)
		isc_event_free(ISC_EVENT_PTR(&event));

	/*
	 * AA bit.
	 */
	if (client->query.restarts == 0 && !authoritative) {
		/*
		 * We're not authoritative, so we must ensure the AA bit
		 * isn't set.
		 */
		client->message->flags &= ~DNS_MESSAGEFLAG_AA;
	}

	/*
	 * Restart the query?
	 */
	if (want_restart && client->query.restarts < MAX_RESTARTS) {
		client->query.restarts++;
		goto restart;
	}

	if (eresult != ISC_R_SUCCESS &&
	    (!PARTIALANSWER(client) || WANTRECURSION(client))) {
		if (eresult == DNS_R_DUPLICATE || eresult == DNS_R_DROP) {
			/*
			 * This was a duplicate query that we are
			 * recursing on.  Don't send a response now.
			 * The original query will still cause a response.
			 */
			query_next(client, eresult);
		} else {
			/*
			 * If we don't have any answer to give the client,
			 * or if the client requested recursion and thus wanted
			 * the complete answer, send an error response.
			 */
			query_error(client, eresult);
		}
		ns_client_detach(&client);
	} else if (!RECURSING(client)) {
		/*
		 * We are done.  Set up sortlist data for the message
		 * rendering code, make a final tweak to the AA bit if the
		 * auth-nxdomain config option says so, then render and
		 * send the response.
		 */
		setup_query_sortlist(client);

		/*
		 * If this is a referral and the answer to the question
		 * is in the glue sort it to the start of the additional
		 * section.
		 */
		if (client->message->counts[DNS_SECTION_ANSWER] == 0 &&
		    client->message->rcode == dns_rcode_noerror &&
		    (qtype == dns_rdatatype_a || qtype == dns_rdatatype_aaaa))
			answer_in_glue(client, qtype);

		if (client->message->rcode == dns_rcode_nxdomain &&
		    client->view->auth_nxdomain == ISC_TRUE)
			client->message->flags |= DNS_MESSAGEFLAG_AA;

		query_send(client);
		ns_client_detach(&client);
	}
	CTRACE("query_find: done");
}

static inline void
log_query(ns_client_t *client) {
	char namebuf[DNS_NAME_FORMATSIZE];
	char typename[DNS_RDATATYPE_FORMATSIZE];
	char classname[DNS_RDATACLASS_FORMATSIZE];
	dns_rdataset_t *rdataset;
	int level = ISC_LOG_INFO;

	if (! isc_log_wouldlog(ns_g_lctx, level))
		return;

	rdataset = ISC_LIST_HEAD(client->query.qname->list);
	INSIST(rdataset != NULL);
	dns_name_format(client->query.qname, namebuf, sizeof(namebuf));
	dns_rdataclass_format(rdataset->rdclass, classname, sizeof(classname));
	dns_rdatatype_format(rdataset->type, typename, sizeof(typename));

	ns_client_log(client, NS_LOGCATEGORY_QUERIES, NS_LOGMODULE_QUERY,
		      level, "query: %s %s %s %s%s%s", namebuf, classname,
		      typename, WANTRECURSION(client) ? "+" : "-",
		      (client->signer != NULL) ? "S": "",
		      (client->opt != NULL) ? "E" : "");
}

void
ns_query_start(ns_client_t *client) {
	isc_result_t result;
	dns_message_t *message = client->message;
	dns_rdataset_t *rdataset;
	ns_client_t *qclient;
	dns_rdatatype_t qtype;

	CTRACE("ns_query_start");

	/*
	 * Ensure that appropriate cleanups occur.
	 */
	client->next = query_next_callback;

	/*
	 * Behave as if we don't support DNSSEC if not enabled.
	 */
	if (!client->view->enablednssec) {
		message->flags &= ~DNS_MESSAGEFLAG_CD;
		client->extflags &= ~DNS_MESSAGEEXTFLAG_DO;
		if (client->opt != NULL)
			client->opt->ttl &= ~DNS_MESSAGEEXTFLAG_DO;
	}

	if ((message->flags & DNS_MESSAGEFLAG_RD) != 0)
		client->query.attributes |= NS_QUERYATTR_WANTRECURSION;

	if ((client->extflags & DNS_MESSAGEEXTFLAG_DO) != 0)
		client->attributes |= NS_CLIENTATTR_WANTDNSSEC;

	if (client->view->minimalresponses)
		client->query.attributes |= (NS_QUERYATTR_NOAUTHORITY |
					     NS_QUERYATTR_NOADDITIONAL);

	if ((client->view->cachedb == NULL)
	    || (!client->view->additionalfromcache)) {
		/*
		 * We don't have a cache.  Turn off cache support and
		 * recursion.
		 */
		client->query.attributes &=
			~(NS_QUERYATTR_RECURSIONOK|NS_QUERYATTR_CACHEOK);
	} else if ((client->attributes & NS_CLIENTATTR_RA) == 0 ||
		   (message->flags & DNS_MESSAGEFLAG_RD) == 0) {
		/*
		 * If the client isn't allowed to recurse (due to
		 * "recursion no", the allow-recursion ACL, or the
		 * lack of a resolver in this view), or if it
		 * doesn't want recursion, turn recursion off.
		 */
		client->query.attributes &= ~NS_QUERYATTR_RECURSIONOK;
	}

	/*
	 * Get the question name.
	 */
	result = dns_message_firstname(message, DNS_SECTION_QUESTION);
	if (result != ISC_R_SUCCESS) {
		query_error(client, result);
		return;
	}
	dns_message_currentname(message, DNS_SECTION_QUESTION,
				&client->query.qname);
	client->query.origqname = client->query.qname;
	result = dns_message_nextname(message, DNS_SECTION_QUESTION);
	if (result != ISC_R_NOMORE) {
		if (result == ISC_R_SUCCESS) {
			/*
			 * There's more than one QNAME in the question
			 * section.
			 */
			query_error(client, DNS_R_FORMERR);
		} else
			query_error(client, result);
		return;
	}

	if (ns_g_server->log_queries)
		log_query(client);

	/*
	 * Check for multiple question queries, since edns1 is dead.
	 */
	if (message->counts[DNS_SECTION_QUESTION] > 1) {
		query_error(client, DNS_R_FORMERR);
		return;
	}

	/*
	 * Check for meta-queries like IXFR and AXFR.
	 */
	rdataset = ISC_LIST_HEAD(client->query.qname->list);
	INSIST(rdataset != NULL);
	qtype = rdataset->type;
	if (dns_rdatatype_ismeta(qtype)) {
		switch (qtype) {
		case dns_rdatatype_any:
			break; /* Let query_find handle it. */
		case dns_rdatatype_ixfr:
		case dns_rdatatype_axfr:
			ns_xfr_start(client, rdataset->type);
			return;
		case dns_rdatatype_maila:
		case dns_rdatatype_mailb:
			query_error(client, DNS_R_NOTIMP);
			return;
		case dns_rdatatype_tkey:
			result = dns_tkey_processquery(client->message,
						ns_g_server->tkeyctx,
						client->view->dynamickeys);
			if (result == ISC_R_SUCCESS)
				query_send(client);
			else
				query_error(client, result);
			return;
		default: /* TSIG, etc. */
			query_error(client, DNS_R_FORMERR);
			return;
		}
	}

	/*
	 * If the client has requested that DNSSEC checking be disabled,
	 * allow lookups to return pending data and instruct the resolver
	 * to return data before validation has completed.
	 *
	 * We don't need to set DNS_DBFIND_PENDINGOK when validation is
	 * disabled as there will be no pending data.
	 */
	if (message->flags & DNS_MESSAGEFLAG_CD ||
	    qtype == dns_rdatatype_rrsig)
	{
		client->query.dboptions |= DNS_DBFIND_PENDINGOK;
		client->query.fetchoptions |= DNS_FETCHOPT_NOVALIDATE;
	} else if (!client->view->enablevalidation)
		client->query.fetchoptions |= DNS_FETCHOPT_NOVALIDATE;

	/*
	 * Allow glue NS records to be added to the authority section
	 * if the answer is secure.
	 */
	if (message->flags & DNS_MESSAGEFLAG_CD)
		client->query.attributes &= ~NS_QUERYATTR_SECURE;

	/*
	 * This is an ordinary query.
	 */
	result = dns_message_reply(message, ISC_TRUE);
	if (result != ISC_R_SUCCESS) {
		query_next(client, result);
		return;
	}

	/*
	 * Assume authoritative response until it is known to be
	 * otherwise.
	 */
	message->flags |= DNS_MESSAGEFLAG_AA;

	/*
	 * Set AD.  We must clear it if we add non-validated data to a
	 * response.
	 */
	if (WANTDNSSEC(client))
		message->flags |= DNS_MESSAGEFLAG_AD;

	qclient = NULL;
	ns_client_attach(client, &qclient);
	query_find(qclient, NULL, qtype);
}
