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

/* $Id: update.c,v 1.199 2011/12/22 07:32:40 each Exp $ */

#include <config.h>

#include <isc/netaddr.h>
#include <isc/print.h>
#include <isc/serial.h>
#include <isc/stats.h>
#include <isc/string.h>
#include <isc/taskpool.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/diff.h>
#include <dns/dnssec.h>
#include <dns/events.h>
#include <dns/fixedname.h>
#include <dns/journal.h>
#include <dns/keyvalues.h>
#include <dns/message.h>
#include <dns/nsec.h>
#include <dns/nsec3.h>
#include <dns/private.h>
#include <dns/rdataclass.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/soa.h>
#include <dns/ssu.h>
#include <dns/tsig.h>
#include <dns/update.h>
#include <dns/view.h>
#include <dns/zone.h>
#include <dns/zt.h>

#include <named/client.h>
#include <named/log.h>
#include <named/server.h>
#include <named/update.h>

/*! \file
 * \brief
 * This module implements dynamic update as in RFC2136.
 */

/*
 *  XXX TODO:
 * - document strict minimality
 */

/**************************************************************************/

/*%
 * Log level for tracing dynamic update protocol requests.
 */
#define LOGLEVEL_PROTOCOL	ISC_LOG_INFO

/*%
 * Log level for low-level debug tracing.
 */
#define LOGLEVEL_DEBUG		ISC_LOG_DEBUG(8)

/*%
 * Check an operation for failure.  These macros all assume that
 * the function using them has a 'result' variable and a 'failure'
 * label.
 */
#define CHECK(op) \
	do { result = (op); \
		if (result != ISC_R_SUCCESS) goto failure; \
	} while (0)

/*%
 * Fail unconditionally with result 'code', which must not
 * be ISC_R_SUCCESS.  The reason for failure presumably has
 * been logged already.
 *
 * The test against ISC_R_SUCCESS is there to keep the Solaris compiler
 * from complaining about "end-of-loop code not reached".
 */

#define FAIL(code) \
	do {							\
		result = (code);				\
		if (result != ISC_R_SUCCESS) goto failure;	\
	} while (0)

/*%
 * Fail unconditionally and log as a client error.
 * The test against ISC_R_SUCCESS is there to keep the Solaris compiler
 * from complaining about "end-of-loop code not reached".
 */
#define FAILC(code, msg) \
	do {							\
		const char *_what = "failed";			\
		result = (code);				\
		switch (result) {				\
		case DNS_R_NXDOMAIN:				\
		case DNS_R_YXDOMAIN:				\
		case DNS_R_YXRRSET:				\
		case DNS_R_NXRRSET:				\
			_what = "unsuccessful";			\
		}						\
		update_log(client, zone, LOGLEVEL_PROTOCOL,	\
			   "update %s: %s (%s)", _what,		\
			   msg, isc_result_totext(result));	\
		if (result != ISC_R_SUCCESS) goto failure;	\
	} while (0)
#define PREREQFAILC(code, msg) \
	do {							\
		inc_stats(zone, dns_nsstatscounter_updatebadprereq); \
		FAILC(code, msg);				\
	} while (0)

#define FAILN(code, name, msg) \
	do {								\
		const char *_what = "failed";				\
		result = (code);					\
		switch (result) {					\
		case DNS_R_NXDOMAIN:					\
		case DNS_R_YXDOMAIN:					\
		case DNS_R_YXRRSET:					\
		case DNS_R_NXRRSET:					\
			_what = "unsuccessful";				\
		}							\
		if (isc_log_wouldlog(ns_g_lctx, LOGLEVEL_PROTOCOL)) {	\
			char _nbuf[DNS_NAME_FORMATSIZE];		\
			dns_name_format(name, _nbuf, sizeof(_nbuf));	\
			update_log(client, zone, LOGLEVEL_PROTOCOL,	\
				   "update %s: %s: %s (%s)", _what, _nbuf, \
				   msg, isc_result_totext(result));	\
		}							\
		if (result != ISC_R_SUCCESS) goto failure;		\
	} while (0)
#define PREREQFAILN(code, name, msg) \
	do {								\
		inc_stats(zone, dns_nsstatscounter_updatebadprereq); \
		FAILN(code, name, msg);					\
	} while (0)

#define FAILNT(code, name, type, msg) \
	do {								\
		const char *_what = "failed";				\
		result = (code);					\
		switch (result) {					\
		case DNS_R_NXDOMAIN:					\
		case DNS_R_YXDOMAIN:					\
		case DNS_R_YXRRSET:					\
		case DNS_R_NXRRSET:					\
			_what = "unsuccessful";				\
		}							\
		if (isc_log_wouldlog(ns_g_lctx, LOGLEVEL_PROTOCOL)) {	\
			char _nbuf[DNS_NAME_FORMATSIZE];		\
			char _tbuf[DNS_RDATATYPE_FORMATSIZE];		\
			dns_name_format(name, _nbuf, sizeof(_nbuf));	\
			dns_rdatatype_format(type, _tbuf, sizeof(_tbuf)); \
			update_log(client, zone, LOGLEVEL_PROTOCOL,	\
				   "update %s: %s/%s: %s (%s)",		\
				   _what, _nbuf, _tbuf, msg,		\
				   isc_result_totext(result));		\
		}							\
		if (result != ISC_R_SUCCESS) goto failure;		\
	} while (0)
#define PREREQFAILNT(code, name, type, msg)				\
	do {								\
		inc_stats(zone, dns_nsstatscounter_updatebadprereq); \
		FAILNT(code, name, type, msg);				\
	} while (0)

/*%
 * Fail unconditionally and log as a server error.
 * The test against ISC_R_SUCCESS is there to keep the Solaris compiler
 * from complaining about "end-of-loop code not reached".
 */
#define FAILS(code, msg) \
	do {							\
		result = (code);				\
		update_log(client, zone, LOGLEVEL_PROTOCOL,	\
			   "error: %s: %s",			\
			   msg, isc_result_totext(result));	\
		if (result != ISC_R_SUCCESS) goto failure;	\
	} while (0)

/*
 * Return TRUE if NS_CLIENTATTR_TCP is set in the attributes other FALSE.
 */
#define TCPCLIENT(client) (((client)->attributes & NS_CLIENTATTR_TCP) != 0)

/**************************************************************************/

typedef struct rr rr_t;

struct rr {
	/* dns_name_t name; */
	isc_uint32_t		ttl;
	dns_rdata_t		rdata;
};

typedef struct update_event update_event_t;

struct update_event {
	ISC_EVENT_COMMON(update_event_t);
	dns_zone_t		*zone;
	isc_result_t		result;
	dns_message_t		*answer;
};

/**************************************************************************/
/*
 * Forward declarations.
 */

static void update_action(isc_task_t *task, isc_event_t *event);
static void updatedone_action(isc_task_t *task, isc_event_t *event);
static isc_result_t send_forward_event(ns_client_t *client, dns_zone_t *zone);
static void forward_done(isc_task_t *task, isc_event_t *event);

/**************************************************************************/

static void
update_log(ns_client_t *client, dns_zone_t *zone,
	   int level, const char *fmt, ...) ISC_FORMAT_PRINTF(4, 5);

static void
update_log(ns_client_t *client, dns_zone_t *zone,
	   int level, const char *fmt, ...)
{
	va_list ap;
	char message[4096];
	char namebuf[DNS_NAME_FORMATSIZE];
	char classbuf[DNS_RDATACLASS_FORMATSIZE];

	if (client == NULL || zone == NULL)
		return;

	if (isc_log_wouldlog(ns_g_lctx, level) == ISC_FALSE)
		return;

	dns_name_format(dns_zone_getorigin(zone), namebuf,
			sizeof(namebuf));
	dns_rdataclass_format(dns_zone_getclass(zone), classbuf,
			      sizeof(classbuf));

	va_start(ap, fmt);
	vsnprintf(message, sizeof(message), fmt, ap);
	va_end(ap);

	ns_client_log(client, NS_LOGCATEGORY_UPDATE, NS_LOGMODULE_UPDATE,
		      level, "updating zone '%s/%s': %s",
		      namebuf, classbuf, message);
}

static void
update_log_cb(void *arg, dns_zone_t *zone, int level, const char *message) {
	update_log(arg, zone, level, "%s", message);
}

/*%
 * Increment updated-related statistics counters.
 */
static inline void
inc_stats(dns_zone_t *zone, isc_statscounter_t counter) {
	isc_stats_increment(ns_g_server->nsstats, counter);

	if (zone != NULL) {
		isc_stats_t *zonestats = dns_zone_getrequeststats(zone);
		if (zonestats != NULL)
			isc_stats_increment(zonestats, counter);
	}
}

/*%
 * Check if we could have queried for the contents of this zone or
 * if the zone is potentially updateable.
 * If the zone can potentially be updated and the check failed then
 * log a error otherwise we log a informational message.
 */
static isc_result_t
checkqueryacl(ns_client_t *client, dns_acl_t *queryacl, dns_name_t *zonename,
	      dns_acl_t *updateacl, dns_ssutable_t *ssutable)
{
	char namebuf[DNS_NAME_FORMATSIZE];
	char classbuf[DNS_RDATACLASS_FORMATSIZE];
	int level;
	isc_result_t result;

	result = ns_client_checkaclsilent(client, NULL, queryacl, ISC_TRUE);
	if (result != ISC_R_SUCCESS) {
		dns_name_format(zonename, namebuf, sizeof(namebuf));
		dns_rdataclass_format(client->view->rdclass, classbuf,
				      sizeof(classbuf));

		level = (updateacl == NULL && ssutable == NULL) ?
				ISC_LOG_INFO : ISC_LOG_ERROR;

		ns_client_log(client, NS_LOGCATEGORY_UPDATE_SECURITY,
			      NS_LOGMODULE_UPDATE, level,
			      "update '%s/%s' denied due to allow-query",
			      namebuf, classbuf);
	} else if (updateacl == NULL && ssutable == NULL) {
		dns_name_format(zonename, namebuf, sizeof(namebuf));
		dns_rdataclass_format(client->view->rdclass, classbuf,
				      sizeof(classbuf));

		result = DNS_R_REFUSED;
		ns_client_log(client, NS_LOGCATEGORY_UPDATE_SECURITY,
			      NS_LOGMODULE_UPDATE, ISC_LOG_INFO,
			      "update '%s/%s' denied", namebuf, classbuf);
	}
	return (result);
}

/*%
 * Override the default acl logging when checking whether a client
 * can update the zone or whether we can forward the request to the
 * master based on IP address.
 *
 * 'message' contains the type of operation that is being attempted.
 * 'slave' indicates if this is a slave zone.  If 'acl' is NULL then
 * log at debug=3.
 * If the zone has no access controls configured ('acl' == NULL &&
 * 'has_ssutable == ISC_FALS) log the attempt at info, otherwise
 * at error.
 *
 * If the request was signed log that we received it.
 */
static isc_result_t
checkupdateacl(ns_client_t *client, dns_acl_t *acl, const char *message,
	       dns_name_t *zonename, isc_boolean_t slave,
	       isc_boolean_t has_ssutable)
{
	char namebuf[DNS_NAME_FORMATSIZE];
	char classbuf[DNS_RDATACLASS_FORMATSIZE];
	int level = ISC_LOG_ERROR;
	const char *msg = "denied";
	isc_result_t result;

	if (slave && acl == NULL) {
		result = DNS_R_NOTIMP;
		level = ISC_LOG_DEBUG(3);
		msg = "disabled";
	} else {
		result = ns_client_checkaclsilent(client, NULL, acl, ISC_FALSE);
		if (result == ISC_R_SUCCESS) {
			level = ISC_LOG_DEBUG(3);
			msg = "approved";
		} else if (acl == NULL && !has_ssutable) {
			level = ISC_LOG_INFO;
		}
	}

	if (client->signer != NULL) {
		dns_name_format(client->signer, namebuf, sizeof(namebuf));
		ns_client_log(client, NS_LOGCATEGORY_UPDATE_SECURITY,
			      NS_LOGMODULE_UPDATE, ISC_LOG_INFO,
			      "signer \"%s\" %s", namebuf, msg);
	}

	dns_name_format(zonename, namebuf, sizeof(namebuf));
	dns_rdataclass_format(client->view->rdclass, classbuf,
			      sizeof(classbuf));

	ns_client_log(client, NS_LOGCATEGORY_UPDATE_SECURITY,
		      NS_LOGMODULE_UPDATE, level, "%s '%s/%s' %s",
		      message, namebuf, classbuf, msg);
	return (result);
}

/*%
 * Update a single RR in version 'ver' of 'db' and log the
 * update in 'diff'.
 *
 * Ensures:
 * \li	'*tuple' == NULL.  Either the tuple is freed, or its
 *	ownership has been transferred to the diff.
 */
static isc_result_t
do_one_tuple(dns_difftuple_t **tuple, dns_db_t *db, dns_dbversion_t *ver,
	     dns_diff_t *diff)
{
	dns_diff_t temp_diff;
	isc_result_t result;

	/*
	 * Create a singleton diff.
	 */
	dns_diff_init(diff->mctx, &temp_diff);
	ISC_LIST_APPEND(temp_diff.tuples, *tuple, link);

	/*
	 * Apply it to the database.
	 */
	result = dns_diff_apply(&temp_diff, db, ver);
	ISC_LIST_UNLINK(temp_diff.tuples, *tuple, link);
	if (result != ISC_R_SUCCESS) {
		dns_difftuple_free(tuple);
		return (result);
	}

	/*
	 * Merge it into the current pending journal entry.
	 */
	dns_diff_appendminimal(diff, tuple);

	/*
	 * Do not clear temp_diff.
	 */
	return (ISC_R_SUCCESS);
}

/*%
 * Perform the updates in 'updates' in version 'ver' of 'db' and log the
 * update in 'diff'.
 *
 * Ensures:
 * \li	'updates' is empty.
 */
static isc_result_t
do_diff(dns_diff_t *updates, dns_db_t *db, dns_dbversion_t *ver,
	dns_diff_t *diff)
{
	isc_result_t result;
	while (! ISC_LIST_EMPTY(updates->tuples)) {
		dns_difftuple_t *t = ISC_LIST_HEAD(updates->tuples);
		ISC_LIST_UNLINK(updates->tuples, t, link);
		CHECK(do_one_tuple(&t, db, ver, diff));
	}
	return (ISC_R_SUCCESS);

 failure:
	dns_diff_clear(diff);
	return (result);
}

static isc_result_t
update_one_rr(dns_db_t *db, dns_dbversion_t *ver, dns_diff_t *diff,
	      dns_diffop_t op, dns_name_t *name, dns_ttl_t ttl,
	      dns_rdata_t *rdata)
{
	dns_difftuple_t *tuple = NULL;
	isc_result_t result;
	result = dns_difftuple_create(diff->mctx, op,
				      name, ttl, rdata, &tuple);
	if (result != ISC_R_SUCCESS)
		return (result);
	return (do_one_tuple(&tuple, db, ver, diff));
}

/**************************************************************************/
/*
 * Callback-style iteration over rdatasets and rdatas.
 *
 * foreach_rrset() can be used to iterate over the RRsets
 * of a name and call a callback function with each
 * one.  Similarly, foreach_rr() can be used to iterate
 * over the individual RRs at name, optionally restricted
 * to RRs of a given type.
 *
 * The callback functions are called "actions" and take
 * two arguments: a void pointer for passing arbitrary
 * context information, and a pointer to the current RRset
 * or RR.  By convention, their names end in "_action".
 */

/*
 * XXXRTH  We might want to make this public somewhere in libdns.
 */

/*%
 * Function type for foreach_rrset() iterator actions.
 */
typedef isc_result_t rrset_func(void *data, dns_rdataset_t *rrset);

/*%
 * Function type for foreach_rr() iterator actions.
 */
typedef isc_result_t rr_func(void *data, rr_t *rr);

/*%
 * Internal context struct for foreach_node_rr().
 */
typedef struct {
	rr_func *	rr_action;
	void *		rr_action_data;
} foreach_node_rr_ctx_t;

/*%
 * Internal helper function for foreach_node_rr().
 */
static isc_result_t
foreach_node_rr_action(void *data, dns_rdataset_t *rdataset) {
	isc_result_t result;
	foreach_node_rr_ctx_t *ctx = data;
	for (result = dns_rdataset_first(rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(rdataset))
	{
		rr_t rr = { 0, DNS_RDATA_INIT };

		dns_rdataset_current(rdataset, &rr.rdata);
		rr.ttl = rdataset->ttl;
		result = (*ctx->rr_action)(ctx->rr_action_data, &rr);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	if (result != ISC_R_NOMORE)
		return (result);
	return (ISC_R_SUCCESS);
}

/*%
 * For each rdataset of 'name' in 'ver' of 'db', call 'action'
 * with the rdataset and 'action_data' as arguments.  If the name
 * does not exist, do nothing.
 *
 * If 'action' returns an error, abort iteration and return the error.
 */
static isc_result_t
foreach_rrset(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	      rrset_func *action, void *action_data)
{
	isc_result_t result;
	dns_dbnode_t *node;
	dns_rdatasetiter_t *iter;

	node = NULL;
	result = dns_db_findnode(db, name, ISC_FALSE, &node);
	if (result == ISC_R_NOTFOUND)
		return (ISC_R_SUCCESS);
	if (result != ISC_R_SUCCESS)
		return (result);

	iter = NULL;
	result = dns_db_allrdatasets(db, node, ver,
				     (isc_stdtime_t) 0, &iter);
	if (result != ISC_R_SUCCESS)
		goto cleanup_node;

	for (result = dns_rdatasetiter_first(iter);
	     result == ISC_R_SUCCESS;
	     result = dns_rdatasetiter_next(iter))
	{
		dns_rdataset_t rdataset;

		dns_rdataset_init(&rdataset);
		dns_rdatasetiter_current(iter, &rdataset);

		result = (*action)(action_data, &rdataset);

		dns_rdataset_disassociate(&rdataset);
		if (result != ISC_R_SUCCESS)
			goto cleanup_iterator;
	}
	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;

 cleanup_iterator:
	dns_rdatasetiter_destroy(&iter);

 cleanup_node:
	dns_db_detachnode(db, &node);

	return (result);
}

/*%
 * For each RR of 'name' in 'ver' of 'db', call 'action'
 * with the RR and 'action_data' as arguments.  If the name
 * does not exist, do nothing.
 *
 * If 'action' returns an error, abort iteration
 * and return the error.
 */
static isc_result_t
foreach_node_rr(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
		rr_func *rr_action, void *rr_action_data)
{
	foreach_node_rr_ctx_t ctx;
	ctx.rr_action = rr_action;
	ctx.rr_action_data = rr_action_data;
	return (foreach_rrset(db, ver, name,
			      foreach_node_rr_action, &ctx));
}


/*%
 * For each of the RRs specified by 'db', 'ver', 'name', 'type',
 * (which can be dns_rdatatype_any to match any type), and 'covers', call
 * 'action' with the RR and 'action_data' as arguments. If the name
 * does not exist, or if no RRset of the given type exists at the name,
 * do nothing.
 *
 * If 'action' returns an error, abort iteration and return the error.
 */
static isc_result_t
foreach_rr(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	   dns_rdatatype_t type, dns_rdatatype_t covers, rr_func *rr_action,
	   void *rr_action_data)
{

	isc_result_t result;
	dns_dbnode_t *node;
	dns_rdataset_t rdataset;

	if (type == dns_rdatatype_any)
		return (foreach_node_rr(db, ver, name,
					rr_action, rr_action_data));

	node = NULL;
	if (type == dns_rdatatype_nsec3 ||
	    (type == dns_rdatatype_rrsig && covers == dns_rdatatype_nsec3))
		result = dns_db_findnsec3node(db, name, ISC_FALSE, &node);
	else
		result = dns_db_findnode(db, name, ISC_FALSE, &node);
	if (result == ISC_R_NOTFOUND)
		return (ISC_R_SUCCESS);
	if (result != ISC_R_SUCCESS)
		return (result);

	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(db, node, ver, type, covers,
				     (isc_stdtime_t) 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND) {
		result = ISC_R_SUCCESS;
		goto cleanup_node;
	}
	if (result != ISC_R_SUCCESS)
		goto cleanup_node;

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset))
	{
		rr_t rr = { 0, DNS_RDATA_INIT };
		dns_rdataset_current(&rdataset, &rr.rdata);
		rr.ttl = rdataset.ttl;
		result = (*rr_action)(rr_action_data, &rr);
		if (result != ISC_R_SUCCESS)
			goto cleanup_rdataset;
	}
	if (result != ISC_R_NOMORE)
		goto cleanup_rdataset;
	result = ISC_R_SUCCESS;

 cleanup_rdataset:
	dns_rdataset_disassociate(&rdataset);
 cleanup_node:
	dns_db_detachnode(db, &node);

	return (result);
}

/**************************************************************************/
/*
 * Various tests on the database contents (for prerequisites, etc).
 */

/*%
 * Function type for predicate functions that compare a database RR 'db_rr'
 * against an update RR 'update_rr'.
 */
typedef isc_boolean_t rr_predicate(dns_rdata_t *update_rr, dns_rdata_t *db_rr);

/*%
 * Helper function for rrset_exists().
 */
static isc_result_t
rrset_exists_action(void *data, rr_t *rr) {
	UNUSED(data);
	UNUSED(rr);
	return (ISC_R_EXISTS);
}

/*%
 * Utility macro for RR existence checking functions.
 *
 * If the variable 'result' has the value ISC_R_EXISTS or
 * ISC_R_SUCCESS, set *exists to ISC_TRUE or ISC_FALSE,
 * respectively, and return success.
 *
 * If 'result' has any other value, there was a failure.
 * Return the failure result code and do not set *exists.
 *
 * This would be more readable as "do { if ... } while(0)",
 * but that form generates tons of warnings on Solaris 2.6.
 */
#define RETURN_EXISTENCE_FLAG				\
	return ((result == ISC_R_EXISTS) ?		\
		(*exists = ISC_TRUE, ISC_R_SUCCESS) :	\
		((result == ISC_R_SUCCESS) ?		\
		 (*exists = ISC_FALSE, ISC_R_SUCCESS) :	\
		 result))

/*%
 * Set '*exists' to true iff an rrset of the given type exists,
 * to false otherwise.
 */
static isc_result_t
rrset_exists(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	     dns_rdatatype_t type, dns_rdatatype_t covers,
	     isc_boolean_t *exists)
{
	isc_result_t result;
	result = foreach_rr(db, ver, name, type, covers,
			    rrset_exists_action, NULL);
	RETURN_EXISTENCE_FLAG;
}

/*%
 * Helper function for cname_incompatible_rrset_exists.
 */
static isc_result_t
cname_compatibility_action(void *data, dns_rdataset_t *rrset) {
	UNUSED(data);
	if (rrset->type != dns_rdatatype_cname &&
	    ! dns_rdatatype_isdnssec(rrset->type))
		return (ISC_R_EXISTS);
	return (ISC_R_SUCCESS);
}

/*%
 * Check whether there is an rrset incompatible with adding a CNAME RR,
 * i.e., anything but another CNAME (which can be replaced) or a
 * DNSSEC RR (which can coexist).
 *
 * If such an incompatible rrset exists, set '*exists' to ISC_TRUE.
 * Otherwise, set it to ISC_FALSE.
 */
static isc_result_t
cname_incompatible_rrset_exists(dns_db_t *db, dns_dbversion_t *ver,
				dns_name_t *name, isc_boolean_t *exists) {
	isc_result_t result;
	result = foreach_rrset(db, ver, name,
			       cname_compatibility_action, NULL);
	RETURN_EXISTENCE_FLAG;
}

/*%
 * Helper function for rr_count().
 */
static isc_result_t
count_rr_action(void *data, rr_t *rr) {
	int *countp = data;
	UNUSED(rr);
	(*countp)++;
	return (ISC_R_SUCCESS);
}

/*%
 * Count the number of RRs of 'type' belonging to 'name' in 'ver' of 'db'.
 */
static isc_result_t
rr_count(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	 dns_rdatatype_t type, dns_rdatatype_t covers, int *countp)
{
	*countp = 0;
	return (foreach_rr(db, ver, name, type, covers,
			   count_rr_action, countp));
}

/*%
 * Context struct and helper function for name_exists().
 */

static isc_result_t
name_exists_action(void *data, dns_rdataset_t *rrset) {
	UNUSED(data);
	UNUSED(rrset);
	return (ISC_R_EXISTS);
}

/*%
 * Set '*exists' to true iff the given name exists, to false otherwise.
 */
static isc_result_t
name_exists(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	    isc_boolean_t *exists)
{
	isc_result_t result;
	result = foreach_rrset(db, ver, name,
			       name_exists_action, NULL);
	RETURN_EXISTENCE_FLAG;
}

/*
 *	'ssu_check_t' is used to pass the arguments to
 *	dns_ssutable_checkrules() to the callback function
 *	ssu_checkrule().
 */
typedef struct {
	/* The ownername of the record to be updated. */
	dns_name_t *name;

	/* The signature's name if the request was signed. */
	dns_name_t *signer;

	/* The address of the client if the request was received via TCP. */
	isc_netaddr_t *tcpaddr;

	/* The ssu table to check against. */
	dns_ssutable_t *table;

	/* the key used for TKEY requests */
	dst_key_t *key;
} ssu_check_t;

static isc_result_t
ssu_checkrule(void *data, dns_rdataset_t *rrset) {
	ssu_check_t *ssuinfo = data;
	isc_boolean_t result;

	/*
	 * If we're deleting all records, it's ok to delete RRSIG and NSEC even
	 * if we're normally not allowed to.
	 */
	if (rrset->type == dns_rdatatype_rrsig ||
	    rrset->type == dns_rdatatype_nsec)
		return (ISC_R_SUCCESS);
	result = dns_ssutable_checkrules(ssuinfo->table, ssuinfo->signer,
					 ssuinfo->name, ssuinfo->tcpaddr,
					 rrset->type, ssuinfo->key);
	return (result == ISC_TRUE ? ISC_R_SUCCESS : ISC_R_FAILURE);
}

static isc_boolean_t
ssu_checkall(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	     dns_ssutable_t *ssutable, dns_name_t *signer,
	     isc_netaddr_t *tcpaddr, dst_key_t *key)
{
	isc_result_t result;
	ssu_check_t ssuinfo;

	ssuinfo.name = name;
	ssuinfo.table = ssutable;
	ssuinfo.signer = signer;
	ssuinfo.tcpaddr = tcpaddr;
	ssuinfo.key = key;
	result = foreach_rrset(db, ver, name, ssu_checkrule, &ssuinfo);
	return (ISC_TF(result == ISC_R_SUCCESS));
}

/**************************************************************************/
/*
 * Checking of "RRset exists (value dependent)" prerequisites.
 *
 * In the RFC2136 section 3.2.5, this is the pseudocode involving
 * a variable called "temp", a mapping of <name, type> tuples to rrsets.
 *
 * Here, we represent the "temp" data structure as (non-minimal) "dns_diff_t"
 * where each tuple has op==DNS_DIFFOP_EXISTS.
 */


/*%
 * Append a tuple asserting the existence of the RR with
 * 'name' and 'rdata' to 'diff'.
 */
static isc_result_t
temp_append(dns_diff_t *diff, dns_name_t *name, dns_rdata_t *rdata) {
	isc_result_t result;
	dns_difftuple_t *tuple = NULL;

	REQUIRE(DNS_DIFF_VALID(diff));
	CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_EXISTS,
				   name, 0, rdata, &tuple));
	ISC_LIST_APPEND(diff->tuples, tuple, link);
 failure:
	return (result);
}

/*%
 * Compare two rdatasets represented as sorted lists of tuples.
 * All list elements must have the same owner name and type.
 * Return ISC_R_SUCCESS if the rdatasets are equal, rcode(dns_rcode_nxrrset)
 * if not.
 */
static isc_result_t
temp_check_rrset(dns_difftuple_t *a, dns_difftuple_t *b) {
	for (;;) {
		if (a == NULL || b == NULL)
			break;
		INSIST(a->op == DNS_DIFFOP_EXISTS &&
		       b->op == DNS_DIFFOP_EXISTS);
		INSIST(a->rdata.type == b->rdata.type);
		INSIST(dns_name_equal(&a->name, &b->name));
		if (dns_rdata_casecompare(&a->rdata, &b->rdata) != 0)
			return (DNS_R_NXRRSET);
		a = ISC_LIST_NEXT(a, link);
		b = ISC_LIST_NEXT(b, link);
	}
	if (a != NULL || b != NULL)
		return (DNS_R_NXRRSET);
	return (ISC_R_SUCCESS);
}

/*%
 * A comparison function defining the sorting order for the entries
 * in the "temp" data structure.  The major sort key is the owner name,
 * followed by the type and rdata.
 */
static int
temp_order(const void *av, const void *bv) {
	dns_difftuple_t const * const *ap = av;
	dns_difftuple_t const * const *bp = bv;
	dns_difftuple_t const *a = *ap;
	dns_difftuple_t const *b = *bp;
	int r;
	r = dns_name_compare(&a->name, &b->name);
	if (r != 0)
		return (r);
	r = (b->rdata.type - a->rdata.type);
	if (r != 0)
		return (r);
	r = dns_rdata_casecompare(&a->rdata, &b->rdata);
	return (r);
}

/*%
 * Check the "RRset exists (value dependent)" prerequisite information
 * in 'temp' against the contents of the database 'db'.
 *
 * Return ISC_R_SUCCESS if the prerequisites are satisfied,
 * rcode(dns_rcode_nxrrset) if not.
 *
 * 'temp' must be pre-sorted.
 */

static isc_result_t
temp_check(isc_mem_t *mctx, dns_diff_t *temp, dns_db_t *db,
	   dns_dbversion_t *ver, dns_name_t *tmpname, dns_rdatatype_t *typep)
{
	isc_result_t result;
	dns_name_t *name;
	dns_dbnode_t *node;
	dns_difftuple_t *t;
	dns_diff_t trash;

	dns_diff_init(mctx, &trash);

	/*
	 * For each name and type in the prerequisites,
	 * construct a sorted rdata list of the corresponding
	 * database contents, and compare the lists.
	 */
	t = ISC_LIST_HEAD(temp->tuples);
	while (t != NULL) {
		name = &t->name;
		(void)dns_name_copy(name, tmpname, NULL);
		*typep = t->rdata.type;

		/* A new unique name begins here. */
		node = NULL;
		result = dns_db_findnode(db, name, ISC_FALSE, &node);
		if (result == ISC_R_NOTFOUND) {
			dns_diff_clear(&trash);
			return (DNS_R_NXRRSET);
		}
		if (result != ISC_R_SUCCESS) {
			dns_diff_clear(&trash);
			return (result);
		}

		/* A new unique type begins here. */
		while (t != NULL && dns_name_equal(&t->name, name)) {
			dns_rdatatype_t type, covers;
			dns_rdataset_t rdataset;
			dns_diff_t d_rrs; /* Database RRs with
						this name and type */
			dns_diff_t u_rrs; /* Update RRs with
						this name and type */

			*typep = type = t->rdata.type;
			if (type == dns_rdatatype_rrsig ||
			    type == dns_rdatatype_sig)
				covers = dns_rdata_covers(&t->rdata);
			else if (type == dns_rdatatype_any) {
				dns_db_detachnode(db, &node);
				dns_diff_clear(&trash);
				return (DNS_R_NXRRSET);
			} else
				covers = 0;

			/*
			 * Collect all database RRs for this name and type
			 * onto d_rrs and sort them.
			 */
			dns_rdataset_init(&rdataset);
			result = dns_db_findrdataset(db, node, ver, type,
						     covers, (isc_stdtime_t) 0,
						     &rdataset, NULL);
			if (result != ISC_R_SUCCESS) {
				dns_db_detachnode(db, &node);
				dns_diff_clear(&trash);
				return (DNS_R_NXRRSET);
			}

			dns_diff_init(mctx, &d_rrs);
			dns_diff_init(mctx, &u_rrs);

			for (result = dns_rdataset_first(&rdataset);
			     result == ISC_R_SUCCESS;
			     result = dns_rdataset_next(&rdataset))
			{
				dns_rdata_t rdata = DNS_RDATA_INIT;
				dns_rdataset_current(&rdataset, &rdata);
				result = temp_append(&d_rrs, name, &rdata);
				if (result != ISC_R_SUCCESS)
					goto failure;
			}
			if (result != ISC_R_NOMORE)
				goto failure;
			result = dns_diff_sort(&d_rrs, temp_order);
			if (result != ISC_R_SUCCESS)
				goto failure;

			/*
			 * Collect all update RRs for this name and type
			 * onto u_rrs.  No need to sort them here -
			 * they are already sorted.
			 */
			while (t != NULL &&
			       dns_name_equal(&t->name, name) &&
			       t->rdata.type == type)
			{
				dns_difftuple_t *next =
					ISC_LIST_NEXT(t, link);
				ISC_LIST_UNLINK(temp->tuples, t, link);
				ISC_LIST_APPEND(u_rrs.tuples, t, link);
				t = next;
			}

			/* Compare the two sorted lists. */
			result = temp_check_rrset(ISC_LIST_HEAD(u_rrs.tuples),
						  ISC_LIST_HEAD(d_rrs.tuples));
			if (result != ISC_R_SUCCESS)
				goto failure;

			/*
			 * We are done with the tuples, but we can't free
			 * them yet because "name" still points into one
			 * of them.  Move them on a temporary list.
			 */
			ISC_LIST_APPENDLIST(trash.tuples, u_rrs.tuples, link);
			ISC_LIST_APPENDLIST(trash.tuples, d_rrs.tuples, link);
			dns_rdataset_disassociate(&rdataset);

			continue;

		    failure:
			dns_diff_clear(&d_rrs);
			dns_diff_clear(&u_rrs);
			dns_diff_clear(&trash);
			dns_rdataset_disassociate(&rdataset);
			dns_db_detachnode(db, &node);
			return (result);
		}

		dns_db_detachnode(db, &node);
	}

	dns_diff_clear(&trash);
	return (ISC_R_SUCCESS);
}

/**************************************************************************/
/*
 * Conditional deletion of RRs.
 */

/*%
 * Context structure for delete_if().
 */

typedef struct {
	rr_predicate *predicate;
	dns_db_t *db;
	dns_dbversion_t *ver;
	dns_diff_t *diff;
	dns_name_t *name;
	dns_rdata_t *update_rr;
} conditional_delete_ctx_t;

/*%
 * Predicate functions for delete_if().
 */

/*%
 * Return true iff 'db_rr' is neither a SOA nor an NS RR nor
 * an RRSIG nor an NSEC3PARAM nor a NSEC.
 */
static isc_boolean_t
type_not_soa_nor_ns_p(dns_rdata_t *update_rr, dns_rdata_t *db_rr) {
	UNUSED(update_rr);
	return ((db_rr->type != dns_rdatatype_soa &&
		 db_rr->type != dns_rdatatype_ns &&
		 db_rr->type != dns_rdatatype_nsec3param &&
		 db_rr->type != dns_rdatatype_rrsig &&
		 db_rr->type != dns_rdatatype_nsec) ?
		ISC_TRUE : ISC_FALSE);
}

/*%
 * Return true iff 'db_rr' is neither a RRSIG nor a NSEC.
 */
static isc_boolean_t
type_not_dnssec(dns_rdata_t *update_rr, dns_rdata_t *db_rr) {
	UNUSED(update_rr);
	return ((db_rr->type != dns_rdatatype_rrsig &&
		 db_rr->type != dns_rdatatype_nsec) ?
		ISC_TRUE : ISC_FALSE);
}

/*%
 * Return true always.
 */
static isc_boolean_t
true_p(dns_rdata_t *update_rr, dns_rdata_t *db_rr) {
	UNUSED(update_rr);
	UNUSED(db_rr);
	return (ISC_TRUE);
}

/*%
 * Return true iff the two RRs have identical rdata.
 */
static isc_boolean_t
rr_equal_p(dns_rdata_t *update_rr, dns_rdata_t *db_rr) {
	/*
	 * XXXRTH  This is not a problem, but we should consider creating
	 *         dns_rdata_equal() (that used dns_name_equal()), since it
	 *         would be faster.  Not a priority.
	 */
	return (dns_rdata_casecompare(update_rr, db_rr) == 0 ?
		ISC_TRUE : ISC_FALSE);
}

/*%
 * Return true iff 'update_rr' should replace 'db_rr' according
 * to the special RFC2136 rules for CNAME, SOA, and WKS records.
 *
 * RFC2136 does not mention NSEC or DNAME, but multiple NSECs or DNAMEs
 * make little sense, so we replace those, too.
 *
 * Additionally replace RRSIG that have been generated by the same key
 * for the same type.  This simplifies refreshing a offline KSK by not
 * requiring that the old RRSIG be deleted.  It also simplifies key
 * rollover by only requiring that the new RRSIG be added.
 */
static isc_boolean_t
replaces_p(dns_rdata_t *update_rr, dns_rdata_t *db_rr) {
	dns_rdata_rrsig_t updatesig, dbsig;
	isc_result_t result;

	if (db_rr->type != update_rr->type)
		return (ISC_FALSE);
	if (db_rr->type == dns_rdatatype_cname)
		return (ISC_TRUE);
	if (db_rr->type == dns_rdatatype_dname)
		return (ISC_TRUE);
	if (db_rr->type == dns_rdatatype_soa)
		return (ISC_TRUE);
	if (db_rr->type == dns_rdatatype_nsec)
		return (ISC_TRUE);
	if (db_rr->type == dns_rdatatype_rrsig) {
		/*
		 * Replace existing RRSIG with the same keyid,
		 * covered and algorithm.
		 */
		result = dns_rdata_tostruct(db_rr, &dbsig, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		result = dns_rdata_tostruct(update_rr, &updatesig, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		if (dbsig.keyid == updatesig.keyid &&
		    dbsig.covered == updatesig.covered &&
		    dbsig.algorithm == updatesig.algorithm)
			return (ISC_TRUE);
	}
	if (db_rr->type == dns_rdatatype_wks) {
		/*
		 * Compare the address and protocol fields only.  These
		 * form the first five bytes of the RR data.  Do a
		 * raw binary comparison; unpacking the WKS RRs using
		 * dns_rdata_tostruct() might be cleaner in some ways.
		 */
		INSIST(db_rr->length >= 5 && update_rr->length >= 5);
		return (memcmp(db_rr->data, update_rr->data, 5) == 0 ?
			ISC_TRUE : ISC_FALSE);
	}

	if (db_rr->type == dns_rdatatype_nsec3param) {
		if (db_rr->length != update_rr->length)
			return (ISC_FALSE);
		INSIST(db_rr->length >= 4 && update_rr->length >= 4);
		/*
		 * Replace NSEC3PARAM records that only differ by the
		 * flags field.
		 */
		if (db_rr->data[0] == update_rr->data[0] &&
		    memcmp(db_rr->data+2, update_rr->data+2,
			   update_rr->length - 2) == 0)
			return (ISC_TRUE);
	}
	return (ISC_FALSE);
}

/*%
 * Internal helper function for delete_if().
 */
static isc_result_t
delete_if_action(void *data, rr_t *rr) {
	conditional_delete_ctx_t *ctx = data;
	if ((*ctx->predicate)(ctx->update_rr, &rr->rdata)) {
		isc_result_t result;
		result = update_one_rr(ctx->db, ctx->ver, ctx->diff,
				       DNS_DIFFOP_DEL, ctx->name,
				       rr->ttl, &rr->rdata);
		return (result);
	} else {
		return (ISC_R_SUCCESS);
	}
}

/*%
 * Conditionally delete RRs.  Apply 'predicate' to the RRs
 * specified by 'db', 'ver', 'name', and 'type' (which can
 * be dns_rdatatype_any to match any type).  Delete those
 * RRs for which the predicate returns true, and log the
 * deletions in 'diff'.
 */
static isc_result_t
delete_if(rr_predicate *predicate, dns_db_t *db, dns_dbversion_t *ver,
	  dns_name_t *name, dns_rdatatype_t type, dns_rdatatype_t covers,
	  dns_rdata_t *update_rr, dns_diff_t *diff)
{
	conditional_delete_ctx_t ctx;
	ctx.predicate = predicate;
	ctx.db = db;
	ctx.ver = ver;
	ctx.diff = diff;
	ctx.name = name;
	ctx.update_rr = update_rr;
	return (foreach_rr(db, ver, name, type, covers,
			   delete_if_action, &ctx));
}

/**************************************************************************/
/*%
 * Prepare an RR for the addition of the new RR 'ctx->update_rr',
 * with TTL 'ctx->update_rr_ttl', to its rdataset, by deleting
 * the RRs if it is replaced by the new RR or has a conflicting TTL.
 * The necessary changes are appended to ctx->del_diff and ctx->add_diff;
 * we need to do all deletions before any additions so that we don't run
 * into transient states with conflicting TTLs.
 */

typedef struct {
	dns_db_t *db;
	dns_dbversion_t *ver;
	dns_diff_t *diff;
	dns_name_t *name;
	dns_rdata_t *update_rr;
	dns_ttl_t update_rr_ttl;
	isc_boolean_t ignore_add;
	dns_diff_t del_diff;
	dns_diff_t add_diff;
} add_rr_prepare_ctx_t;

static isc_result_t
add_rr_prepare_action(void *data, rr_t *rr) {
	isc_result_t result = ISC_R_SUCCESS;
	add_rr_prepare_ctx_t *ctx = data;
	dns_difftuple_t *tuple = NULL;
	isc_boolean_t equal;

	/*
	 * If the update RR is a "duplicate" of the update RR,
	 * the update should be silently ignored.
	 */
	equal = ISC_TF(dns_rdata_casecompare(&rr->rdata, ctx->update_rr) == 0);
	if (equal && rr->ttl == ctx->update_rr_ttl) {
		ctx->ignore_add = ISC_TRUE;
		return (ISC_R_SUCCESS);
	}

	/*
	 * If this RR is "equal" to the update RR, it should
	 * be deleted before the update RR is added.
	 */
	if (replaces_p(ctx->update_rr, &rr->rdata)) {
		CHECK(dns_difftuple_create(ctx->del_diff.mctx, DNS_DIFFOP_DEL,
					   ctx->name, rr->ttl, &rr->rdata,
					   &tuple));
		dns_diff_append(&ctx->del_diff, &tuple);
		return (ISC_R_SUCCESS);
	}

	/*
	 * If this RR differs in TTL from the update RR,
	 * its TTL must be adjusted.
	 */
	if (rr->ttl != ctx->update_rr_ttl) {
		CHECK(dns_difftuple_create(ctx->del_diff.mctx, DNS_DIFFOP_DEL,
					   ctx->name, rr->ttl, &rr->rdata,
					   &tuple));
		dns_diff_append(&ctx->del_diff, &tuple);
		if (!equal) {
			CHECK(dns_difftuple_create(ctx->add_diff.mctx,
						   DNS_DIFFOP_ADD, ctx->name,
						   ctx->update_rr_ttl,
						   &rr->rdata, &tuple));
			dns_diff_append(&ctx->add_diff, &tuple);
		}
	}
 failure:
	return (result);
}

/**************************************************************************/
/*
 * Miscellaneous subroutines.
 */

/*%
 * Extract a single update RR from 'section' of dynamic update message
 * 'msg', with consistency checking.
 *
 * Stores the owner name, rdata, and TTL of the update RR at 'name',
 * 'rdata', and 'ttl', respectively.
 */
static void
get_current_rr(dns_message_t *msg, dns_section_t section,
	       dns_rdataclass_t zoneclass, dns_name_t **name,
	       dns_rdata_t *rdata, dns_rdatatype_t *covers,
	       dns_ttl_t *ttl, dns_rdataclass_t *update_class)
{
	dns_rdataset_t *rdataset;
	isc_result_t result;
	dns_message_currentname(msg, section, name);
	rdataset = ISC_LIST_HEAD((*name)->list);
	INSIST(rdataset != NULL);
	INSIST(ISC_LIST_NEXT(rdataset, link) == NULL);
	*covers = rdataset->covers;
	*ttl = rdataset->ttl;
	result = dns_rdataset_first(rdataset);
	INSIST(result == ISC_R_SUCCESS);
	dns_rdataset_current(rdataset, rdata);
	INSIST(dns_rdataset_next(rdataset) == ISC_R_NOMORE);
	*update_class = rdata->rdclass;
	rdata->rdclass = zoneclass;
}

/*%
 * Increment the SOA serial number of database 'db', version 'ver'.
 * Replace the SOA record in the database, and log the
 * change in 'diff'.
 */

	/*
	 * XXXRTH  Failures in this routine will be worth logging, when
	 *         we have a logging system.  Failure to find the zonename
	 *	   or the SOA rdataset warrant at least an UNEXPECTED_ERROR().
	 */

static isc_result_t
update_soa_serial(dns_db_t *db, dns_dbversion_t *ver, dns_diff_t *diff,
		  isc_mem_t *mctx, dns_updatemethod_t method)
{
	dns_difftuple_t *deltuple = NULL;
	dns_difftuple_t *addtuple = NULL;
	isc_uint32_t serial;
	isc_result_t result;

	CHECK(dns_db_createsoatuple(db, ver, mctx, DNS_DIFFOP_DEL, &deltuple));
	CHECK(dns_difftuple_copy(deltuple, &addtuple));
	addtuple->op = DNS_DIFFOP_ADD;

	serial = dns_soa_getserial(&addtuple->rdata);
	serial = dns_update_soaserial(serial, method);
	dns_soa_setserial(serial, &addtuple->rdata);
	CHECK(do_one_tuple(&deltuple, db, ver, diff));
	CHECK(do_one_tuple(&addtuple, db, ver, diff));
	result = ISC_R_SUCCESS;

 failure:
	if (addtuple != NULL)
		dns_difftuple_free(&addtuple);
	if (deltuple != NULL)
		dns_difftuple_free(&deltuple);
	return (result);
}

/*%
 * Check that the new SOA record at 'update_rdata' does not
 * illegally cause the SOA serial number to decrease or stay
 * unchanged relative to the existing SOA in 'db'.
 *
 * Sets '*ok' to ISC_TRUE if the update is legal, ISC_FALSE if not.
 *
 * William King points out that RFC2136 is inconsistent about
 * the case where the serial number stays unchanged:
 *
 *   section 3.4.2.2 requires a server to ignore a SOA update request
 *   if the serial number on the update SOA is less_than_or_equal to
 *   the zone SOA serial.
 *
 *   section 3.6 requires a server to ignore a SOA update request if
 *   the serial is less_than the zone SOA serial.
 *
 * Paul says 3.4.2.2 is correct.
 *
 */
static isc_result_t
check_soa_increment(dns_db_t *db, dns_dbversion_t *ver,
		    dns_rdata_t *update_rdata, isc_boolean_t *ok)
{
	isc_uint32_t db_serial;
	isc_uint32_t update_serial;
	isc_result_t result;

	update_serial = dns_soa_getserial(update_rdata);

	result = dns_db_getsoaserial(db, ver, &db_serial);
	if (result != ISC_R_SUCCESS)
		return (result);

	if (DNS_SERIAL_GE(db_serial, update_serial)) {
		*ok = ISC_FALSE;
	} else {
		*ok = ISC_TRUE;
	}

	return (ISC_R_SUCCESS);

}

/**************************************************************************/
/*%
 * The actual update code in all its glory.  We try to follow
 * the RFC2136 pseudocode as closely as possible.
 */

static isc_result_t
send_update_event(ns_client_t *client, dns_zone_t *zone) {
	isc_result_t result = ISC_R_SUCCESS;
	update_event_t *event = NULL;
	isc_task_t *zonetask = NULL;
	ns_client_t *evclient;

	event = (update_event_t *)
		isc_event_allocate(client->mctx, client, DNS_EVENT_UPDATE,
				   update_action, NULL, sizeof(*event));
	if (event == NULL)
		FAIL(ISC_R_NOMEMORY);
	event->zone = zone;
	event->result = ISC_R_SUCCESS;

	evclient = NULL;
	ns_client_attach(client, &evclient);
	INSIST(client->nupdates == 0);
	client->nupdates++;
	event->ev_arg = evclient;

	dns_zone_gettask(zone, &zonetask);
	isc_task_send(zonetask, ISC_EVENT_PTR(&event));

 failure:
	if (event != NULL)
		isc_event_free(ISC_EVENT_PTR(&event));
	return (result);
}

static void
respond(ns_client_t *client, isc_result_t result) {
	isc_result_t msg_result;

	msg_result = dns_message_reply(client->message, ISC_TRUE);
	if (msg_result != ISC_R_SUCCESS)
		goto msg_failure;
	client->message->rcode = dns_result_torcode(result);

	ns_client_send(client);
	return;

 msg_failure:
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_UPDATE, NS_LOGMODULE_UPDATE,
		      ISC_LOG_ERROR,
		      "could not create update response message: %s",
		      isc_result_totext(msg_result));
	ns_client_next(client, msg_result);
}

void
ns_update_start(ns_client_t *client, isc_result_t sigresult) {
	dns_message_t *request = client->message;
	isc_result_t result;
	dns_name_t *zonename;
	dns_rdataset_t *zone_rdataset;
	dns_zone_t *zone = NULL, *raw = NULL;

	/*
	 * Interpret the zone section.
	 */
	result = dns_message_firstname(request, DNS_SECTION_ZONE);
	if (result != ISC_R_SUCCESS)
		FAILC(DNS_R_FORMERR, "update zone section empty");

	/*
	 * The zone section must contain exactly one "question", and
	 * it must be of type SOA.
	 */
	zonename = NULL;
	dns_message_currentname(request, DNS_SECTION_ZONE, &zonename);
	zone_rdataset = ISC_LIST_HEAD(zonename->list);
	if (zone_rdataset->type != dns_rdatatype_soa)
		FAILC(DNS_R_FORMERR,
		      "update zone section contains non-SOA");
	if (ISC_LIST_NEXT(zone_rdataset, link) != NULL)
		FAILC(DNS_R_FORMERR,
		      "update zone section contains multiple RRs");

	/* The zone section must have exactly one name. */
	result = dns_message_nextname(request, DNS_SECTION_ZONE);
	if (result != ISC_R_NOMORE)
		FAILC(DNS_R_FORMERR,
		      "update zone section contains multiple RRs");

	result = dns_zt_find(client->view->zonetable, zonename, 0, NULL,
			     &zone);
	if (result != ISC_R_SUCCESS)
		FAILC(DNS_R_NOTAUTH, "not authoritative for update zone");

	/*
	 * If there is a raw (unsigned) zone associated with this
	 * zone then it processes the UPDATE request.
	 */
	dns_zone_getraw(zone, &raw);
	if (raw != NULL) {
		dns_zone_detach(&zone);
		dns_zone_attach(raw, &zone);
		dns_zone_detach(&raw);
	}

	switch(dns_zone_gettype(zone)) {
	case dns_zone_master:
	case dns_zone_dlz:
		/*
		 * We can now fail due to a bad signature as we now know
		 * that we are the master.
		 */
		if (sigresult != ISC_R_SUCCESS)
			FAIL(sigresult);
		CHECK(send_update_event(client, zone));
		break;
	case dns_zone_slave:
		CHECK(checkupdateacl(client, dns_zone_getforwardacl(zone),
				     "update forwarding", zonename, ISC_TRUE,
				     ISC_FALSE));
		CHECK(send_forward_event(client, zone));
		break;
	default:
		FAILC(DNS_R_NOTAUTH, "not authoritative for update zone");
	}
	return;

 failure:
	if (result == DNS_R_REFUSED) {
		INSIST(dns_zone_gettype(zone) == dns_zone_slave);
		inc_stats(zone, dns_nsstatscounter_updaterej);
	}
	/*
	 * We failed without having sent an update event to the zone.
	 * We are still in the client task context, so we can
	 * simply give an error response without switching tasks.
	 */
	respond(client, result);
	if (zone != NULL)
		dns_zone_detach(&zone);
}

/*%
 * DS records are not allowed to exist without corresponding NS records,
 * RFC 3658, 2.2 Protocol Change,
 * "DS RRsets MUST NOT appear at non-delegation points or at a zone's apex".
 */

static isc_result_t
remove_orphaned_ds(dns_db_t *db, dns_dbversion_t *newver, dns_diff_t *diff) {
	isc_result_t result;
	isc_boolean_t ns_exists;
	dns_difftuple_t *tupple;
	dns_diff_t temp_diff;

	dns_diff_init(diff->mctx, &temp_diff);

	for (tupple = ISC_LIST_HEAD(diff->tuples);
	     tupple != NULL;
	     tupple = ISC_LIST_NEXT(tupple, link)) {
		if (!((tupple->op == DNS_DIFFOP_DEL &&
		       tupple->rdata.type == dns_rdatatype_ns) ||
		      (tupple->op == DNS_DIFFOP_ADD &&
		       tupple->rdata.type == dns_rdatatype_ds)))
			continue;
		CHECK(rrset_exists(db, newver, &tupple->name,
				   dns_rdatatype_ns, 0, &ns_exists));
		if (ns_exists &&
		    !dns_name_equal(&tupple->name, dns_db_origin(db)))
			continue;
		CHECK(delete_if(true_p, db, newver, &tupple->name,
				dns_rdatatype_ds, 0, NULL, &temp_diff));
	}
	result = ISC_R_SUCCESS;

 failure:
	for (tupple = ISC_LIST_HEAD(temp_diff.tuples);
	     tupple != NULL;
	     tupple = ISC_LIST_HEAD(temp_diff.tuples)) {
		ISC_LIST_UNLINK(temp_diff.tuples, tupple, link);
		dns_diff_appendminimal(diff, &tupple);
	}
	return (result);
}

/*
 * This implements the post load integrity checks for mx records.
 */
static isc_result_t
check_mx(ns_client_t *client, dns_zone_t *zone,
	 dns_db_t *db, dns_dbversion_t *newver, dns_diff_t *diff)
{
	char tmp[sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:123.123.123.123.")];
	char ownerbuf[DNS_NAME_FORMATSIZE];
	char namebuf[DNS_NAME_FORMATSIZE];
	char altbuf[DNS_NAME_FORMATSIZE];
	dns_difftuple_t *t;
	dns_fixedname_t fixed;
	dns_name_t *foundname;
	dns_rdata_mx_t mx;
	dns_rdata_t rdata;
	isc_boolean_t ok = ISC_TRUE;
	isc_boolean_t isaddress;
	isc_result_t result;
	struct in6_addr addr6;
	struct in_addr addr;
	unsigned int options;

	dns_fixedname_init(&fixed);
	foundname = dns_fixedname_name(&fixed);
	dns_rdata_init(&rdata);
	options = dns_zone_getoptions(zone);

	for (t = ISC_LIST_HEAD(diff->tuples);
	     t != NULL;
	     t = ISC_LIST_NEXT(t, link)) {
		if (t->op != DNS_DIFFOP_ADD ||
		    t->rdata.type != dns_rdatatype_mx)
			continue;

		result = dns_rdata_tostruct(&t->rdata, &mx, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		/*
		 * Check if we will error out if we attempt to reload the
		 * zone.
		 */
		dns_name_format(&mx.mx, namebuf, sizeof(namebuf));
		dns_name_format(&t->name, ownerbuf, sizeof(ownerbuf));
		isaddress = ISC_FALSE;
		if ((options & DNS_RDATA_CHECKMX) != 0 &&
		    strlcpy(tmp, namebuf, sizeof(tmp)) < sizeof(tmp)) {
			if (tmp[strlen(tmp) - 1] == '.')
				tmp[strlen(tmp) - 1] = '\0';
			if (inet_aton(tmp, &addr) == 1 ||
			    inet_pton(AF_INET6, tmp, &addr6) == 1)
				isaddress = ISC_TRUE;
		}

		if (isaddress && (options & DNS_RDATA_CHECKMXFAIL) != 0) {
			update_log(client, zone, ISC_LOG_ERROR,
				   "%s/MX: '%s': %s",
				   ownerbuf, namebuf,
				   dns_result_totext(DNS_R_MXISADDRESS));
			ok = ISC_FALSE;
		} else if (isaddress) {
			update_log(client, zone, ISC_LOG_WARNING,
				   "%s/MX: warning: '%s': %s",
				   ownerbuf, namebuf,
				   dns_result_totext(DNS_R_MXISADDRESS));
		}

		/*
		 * Check zone integrity checks.
		 */
		if ((options & DNS_ZONEOPT_CHECKINTEGRITY) == 0)
			continue;
		result = dns_db_find(db, &mx.mx, newver, dns_rdatatype_a,
				     0, 0, NULL, foundname, NULL, NULL);
		if (result == ISC_R_SUCCESS)
			continue;

		if (result == DNS_R_NXRRSET) {
			result = dns_db_find(db, &mx.mx, newver,
					     dns_rdatatype_aaaa,
					     0, 0, NULL, foundname,
					     NULL, NULL);
			if (result == ISC_R_SUCCESS)
				continue;
		}

		if (result == DNS_R_NXRRSET || result == DNS_R_NXDOMAIN) {
			update_log(client, zone, ISC_LOG_ERROR,
				   "%s/MX '%s' has no address records "
				   "(A or AAAA)", ownerbuf, namebuf);
			ok = ISC_FALSE;
		} else if (result == DNS_R_CNAME) {
			update_log(client, zone, ISC_LOG_ERROR,
				   "%s/MX '%s' is a CNAME (illegal)",
				   ownerbuf, namebuf);
			ok = ISC_FALSE;
		} else if (result == DNS_R_DNAME) {
			dns_name_format(foundname, altbuf, sizeof altbuf);
			update_log(client, zone, ISC_LOG_ERROR,
				   "%s/MX '%s' is below a DNAME '%s' (illegal)",
				   ownerbuf, namebuf, altbuf);
			ok = ISC_FALSE;
		}
	}
	return (ok ? ISC_R_SUCCESS : DNS_R_REFUSED);
}

static isc_result_t
rr_exists(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	  const dns_rdata_t *rdata, isc_boolean_t *flag)
{
	dns_rdataset_t rdataset;
	dns_dbnode_t *node = NULL;
	isc_result_t result;

	dns_rdataset_init(&rdataset);
	if (rdata->type == dns_rdatatype_nsec3)
		CHECK(dns_db_findnsec3node(db, name, ISC_FALSE, &node));
	else
		CHECK(dns_db_findnode(db, name, ISC_FALSE, &node));
	result = dns_db_findrdataset(db, node, ver, rdata->type, 0,
				     (isc_stdtime_t) 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND) {
		*flag = ISC_FALSE;
		result = ISC_R_SUCCESS;
		goto failure;
	}

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t myrdata = DNS_RDATA_INIT;
		dns_rdataset_current(&rdataset, &myrdata);
		if (!dns_rdata_casecompare(&myrdata, rdata))
			break;
	}
	dns_rdataset_disassociate(&rdataset);
	if (result == ISC_R_SUCCESS) {
		*flag = ISC_TRUE;
	} else if (result == ISC_R_NOMORE) {
		*flag = ISC_FALSE;
		result = ISC_R_SUCCESS;
	}

 failure:
	if (node != NULL)
		dns_db_detachnode(db, &node);
	return (result);
}

static isc_result_t
get_iterations(dns_db_t *db, dns_dbversion_t *ver, dns_rdatatype_t privatetype,
	       unsigned int *iterationsp)
{
	dns_dbnode_t *node = NULL;
	dns_rdata_nsec3param_t nsec3param;
	dns_rdataset_t rdataset;
	isc_result_t result;
	unsigned int iterations = 0;

	dns_rdataset_init(&rdataset);

	result = dns_db_getoriginnode(db, &node);
	if (result != ISC_R_SUCCESS)
		return (result);
	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_nsec3param,
				     0, (isc_stdtime_t) 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND)
		goto try_private;
	if (result != ISC_R_SUCCESS)
		goto failure;

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;
		dns_rdataset_current(&rdataset, &rdata);
		CHECK(dns_rdata_tostruct(&rdata, &nsec3param, NULL));
		if ((nsec3param.flags & DNS_NSEC3FLAG_REMOVE) != 0)
			continue;
		if (nsec3param.iterations > iterations)
			iterations = nsec3param.iterations;
	}
	if (result != ISC_R_NOMORE)
		goto failure;

	dns_rdataset_disassociate(&rdataset);

 try_private:
	if (privatetype == 0)
		goto success;

	result = dns_db_findrdataset(db, node, ver, privatetype,
				     0, (isc_stdtime_t) 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND)
		goto success;
	if (result != ISC_R_SUCCESS)
		goto failure;

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		unsigned char buf[DNS_NSEC3PARAM_BUFFERSIZE];
		dns_rdata_t private = DNS_RDATA_INIT;
		dns_rdata_t rdata = DNS_RDATA_INIT;

		dns_rdataset_current(&rdataset, &rdata);
		if (!dns_nsec3param_fromprivate(&private, &rdata,
						buf, sizeof(buf)))
			continue;
		CHECK(dns_rdata_tostruct(&rdata, &nsec3param, NULL));
		if ((nsec3param.flags & DNS_NSEC3FLAG_REMOVE) != 0)
			continue;
		if (nsec3param.iterations > iterations)
			iterations = nsec3param.iterations;
	}
	if (result != ISC_R_NOMORE)
		goto failure;

 success:
	*iterationsp = iterations;
	result = ISC_R_SUCCESS;

 failure:
	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	return (result);
}

/*
 * Prevent the zone entering a inconsistent state where
 * NSEC only DNSKEYs are present with NSEC3 chains.
 */
static isc_result_t
check_dnssec(ns_client_t *client, dns_zone_t *zone, dns_db_t *db,
	     dns_dbversion_t *ver, dns_diff_t *diff)
{
	dns_difftuple_t *tuple;
	isc_boolean_t nseconly = ISC_FALSE, nsec3 = ISC_FALSE;
	isc_result_t result;
	unsigned int iterations = 0, max;
	dns_rdatatype_t privatetype = dns_zone_getprivatetype(zone);

	/* Scan the tuples for an NSEC-only DNSKEY or an NSEC3PARAM */
	for (tuple = ISC_LIST_HEAD(diff->tuples);
	     tuple != NULL;
	     tuple = ISC_LIST_NEXT(tuple, link)) {
		if (tuple->op != DNS_DIFFOP_ADD)
			continue;

		if (tuple->rdata.type == dns_rdatatype_dnskey) {
			isc_uint8_t alg;
			alg = tuple->rdata.data[3];
			if (alg == DST_ALG_RSAMD5 || alg == DST_ALG_RSASHA1 ||
			    alg == DST_ALG_DSA || alg == DST_ALG_ECC) {
				nseconly = ISC_TRUE;
				break;
			}
		} else if (tuple->rdata.type == dns_rdatatype_nsec3param) {
			nsec3 = ISC_TRUE;
			break;
		}
	}

	/* Check existing DB for NSEC-only DNSKEY */
	if (!nseconly) {
		result = dns_nsec_nseconly(db, ver, &nseconly);

		/*
		 * An NSEC3PARAM update can proceed without a DNSKEY (it
		 * will trigger a delayed change), so we can ignore
		 * ISC_R_NOTFOUND here.
		 */
		if (result == ISC_R_NOTFOUND)
			result = ISC_R_SUCCESS;

		CHECK(result);
	}

	/* Check existing DB for NSEC3 */
	if (!nsec3)
		CHECK(dns_nsec3_activex(db, ver, ISC_FALSE,
					privatetype, &nsec3));

	/* Refuse to allow NSEC3 with NSEC-only keys */
	if (nseconly && nsec3) {
		update_log(client, zone, ISC_LOG_ERROR,
			   "NSEC only DNSKEYs and NSEC3 chains not allowed");
		result = DNS_R_REFUSED;
		goto failure;
	}

	/* Verify NSEC3 params */
	CHECK(get_iterations(db, ver, privatetype, &iterations));
	CHECK(dns_nsec3_maxiterations(db, ver, client->mctx, &max));
	if (max != 0 && iterations > max) {
		update_log(client, zone, ISC_LOG_ERROR,
			   "too many NSEC3 iterations (%u) for "
			   "weakest DNSKEY (%u)", iterations, max);
		result = DNS_R_REFUSED;
		goto failure;
	}

 failure:
	return (result);
}

/*
 * Delay NSEC3PARAM changes as they need to be applied to the whole zone.
 */
static isc_result_t
add_nsec3param_records(ns_client_t *client, dns_zone_t *zone, dns_db_t *db,
		       dns_dbversion_t *ver, dns_diff_t *diff)
{
	isc_result_t result = ISC_R_SUCCESS;
	dns_difftuple_t *tuple, *newtuple = NULL, *next;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	unsigned char buf[DNS_NSEC3PARAM_BUFFERSIZE + 1];
	dns_diff_t temp_diff;
	dns_diffop_t op;
	isc_boolean_t flag;
	dns_name_t *name = dns_zone_getorigin(zone);
	dns_rdatatype_t privatetype = dns_zone_getprivatetype(zone);
	isc_uint32_t ttl = 0;
	isc_boolean_t ttl_good = ISC_FALSE;

	update_log(client, zone, ISC_LOG_DEBUG(3),
		    "checking for NSEC3PARAM changes");

	dns_diff_init(diff->mctx, &temp_diff);

	/*
	 * Extract NSEC3PARAM tuples from list.
	 */
	for (tuple = ISC_LIST_HEAD(diff->tuples);
	     tuple != NULL;
	     tuple = next) {

		next = ISC_LIST_NEXT(tuple, link);

		if (tuple->rdata.type != dns_rdatatype_nsec3param ||
		    !dns_name_equal(name, &tuple->name))
			continue;
		ISC_LIST_UNLINK(diff->tuples, tuple, link);
		ISC_LIST_APPEND(temp_diff.tuples, tuple, link);
	}

	/*
	 * Extract TTL changes pairs, we don't need to convert these to
	 * delayed changes.
	 */
	for (tuple = ISC_LIST_HEAD(temp_diff.tuples);
	     tuple != NULL; tuple = next) {
		if (tuple->op == DNS_DIFFOP_ADD) {
			if (!ttl_good) {
				/*
				 * Any adds here will contain the final
				 * NSEC3PARAM RRset TTL.
				 */
				ttl = tuple->ttl;
				ttl_good = ISC_TRUE;
			}
			/*
			 * Walk the temp_diff list looking for the
			 * corresponding delete.
			 */
			next = ISC_LIST_HEAD(temp_diff.tuples);
			while (next != NULL) {
				unsigned char *next_data = next->rdata.data;
				unsigned char *tuple_data = tuple->rdata.data;
				if (next->op == DNS_DIFFOP_DEL &&
				    next->rdata.length == tuple->rdata.length &&
				    !memcmp(next_data, tuple_data,
					    next->rdata.length)) {
					ISC_LIST_UNLINK(temp_diff.tuples, next,
							link);
					ISC_LIST_APPEND(diff->tuples, next,
							link);
					break;
				}
				next = ISC_LIST_NEXT(next, link);
			}
			/*
			 * If we have not found a pair move onto the next
			 * tuple.
			 */
			if (next == NULL) {
				next = ISC_LIST_NEXT(tuple, link);
				continue;
			}
			/*
			 * Find the next tuple to be processed before
			 * unlinking then complete moving the pair to 'diff'.
			 */
			next = ISC_LIST_NEXT(tuple, link);
			ISC_LIST_UNLINK(temp_diff.tuples, tuple, link);
			ISC_LIST_APPEND(diff->tuples, tuple, link);
		} else
			next = ISC_LIST_NEXT(tuple, link);
	}

	/*
	 * Preserve any ongoing changes from a BIND 9.6.x upgrade.
	 *
	 * Any NSEC3PARAM records with flags other than OPTOUT named
	 * in managing and should not be touched so revert such changes
	 * taking into account any TTL change of the NSEC3PARAM RRset.
	 */
	for (tuple = ISC_LIST_HEAD(temp_diff.tuples);
	     tuple != NULL; tuple = next) {
		next = ISC_LIST_NEXT(tuple, link);
		if ((tuple->rdata.data[1] & ~DNS_NSEC3FLAG_OPTOUT) != 0) {
			/*
			 * If we havn't had any adds then the tuple->ttl must
			 * be the original ttl and should be used for any
			 * future changes.
			 */
			if (!ttl_good) {
				ttl = tuple->ttl;
				ttl_good = ISC_TRUE;
			}
			op = (tuple->op == DNS_DIFFOP_DEL) ?
			     DNS_DIFFOP_ADD : DNS_DIFFOP_DEL;
			CHECK(dns_difftuple_create(diff->mctx, op, name,
						   ttl, &tuple->rdata,
						   &newtuple));
			CHECK(do_one_tuple(&newtuple, db, ver, diff));
			ISC_LIST_UNLINK(temp_diff.tuples, tuple, link);
			dns_diff_appendminimal(diff, &tuple);
		}
	}

	/*
	 * We now have just the actual changes to the NSEC3PARAM RRset.
	 * Convert the adds to delayed adds and the deletions into delayed
	 * deletions.
	 */
	for (tuple = ISC_LIST_HEAD(temp_diff.tuples);
	     tuple != NULL; tuple = next) {
		/*
		 * If we havn't had any adds then the tuple->ttl must be the
		 * original ttl and should be used for any future changes.
		 */
		if (!ttl_good) {
			ttl = tuple->ttl;
			ttl_good = ISC_TRUE;
		}
		if (tuple->op == DNS_DIFFOP_ADD) {
			isc_boolean_t nseconly = ISC_FALSE;

			/*
			 * Look for any deletes which match this ADD ignoring
			 * flags.  We don't need to explictly remove them as
			 * they will be removed a side effect of processing
			 * the add.
			 */
			next = ISC_LIST_HEAD(temp_diff.tuples);
			while (next != NULL) {
				unsigned char *next_data = next->rdata.data;
				unsigned char *tuple_data = tuple->rdata.data;
				if (next->op != DNS_DIFFOP_DEL ||
				    next->rdata.length != tuple->rdata.length ||
				    next_data[0] != tuple_data[0] ||
				    next_data[2] != tuple_data[2] ||
				    next_data[3] != tuple_data[3] ||
				    memcmp(next_data + 4, tuple_data + 4,
					   tuple->rdata.length - 4)) {
					next = ISC_LIST_NEXT(next, link);
					continue;
				}
				ISC_LIST_UNLINK(temp_diff.tuples, next, link);
				ISC_LIST_APPEND(diff->tuples, next, link);
				next = ISC_LIST_HEAD(temp_diff.tuples);
			}

			/*
			 * Create a private-type record to signal that
			 * we want a delayed NSEC3 chain add/delete
			 */
			dns_nsec3param_toprivate(&tuple->rdata, &rdata,
						 privatetype, buf, sizeof(buf));
			buf[2] |= DNS_NSEC3FLAG_CREATE;

			/*
			 * If the zone is not currently capable of
			 * supporting an NSEC3 chain, then we set the
			 * INITIAL flag to indicate that these parameters
			 * are to be used later.
			 */
			result = dns_nsec_nseconly(db, ver, &nseconly);
			if (result == ISC_R_NOTFOUND || nseconly)
				buf[2] |= DNS_NSEC3FLAG_INITIAL;

			/*
			 * See if this CREATE request already exists.
			 */
			CHECK(rr_exists(db, ver, name, &rdata, &flag));

			if (!flag) {
				CHECK(dns_difftuple_create(diff->mctx,
							   DNS_DIFFOP_ADD,
							   name, 0, &rdata,
							   &newtuple));
				CHECK(do_one_tuple(&newtuple, db, ver, diff));
			}

			/*
			 * Remove any existing CREATE request to add an
			 * otherwise indentical chain with a reversed
			 * OPTOUT state.
			 */
			buf[2] ^= DNS_NSEC3FLAG_OPTOUT;
			CHECK(rr_exists(db, ver, name, &rdata, &flag));

			if (flag) {
				CHECK(dns_difftuple_create(diff->mctx,
							   DNS_DIFFOP_DEL,
							   name, 0, &rdata,
							   &newtuple));
				CHECK(do_one_tuple(&newtuple, db, ver, diff));
			}

			/*
			 * Find the next tuple to be processed and remove the
			 * temporary add record.
			 */
			next = ISC_LIST_NEXT(tuple, link);
			CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_DEL,
						   name, ttl, &tuple->rdata,
						   &newtuple));
			CHECK(do_one_tuple(&newtuple, db, ver, diff));
			ISC_LIST_UNLINK(temp_diff.tuples, tuple, link);
			dns_diff_appendminimal(diff, &tuple);
			dns_rdata_reset(&rdata);
		} else
			next = ISC_LIST_NEXT(tuple, link);
	}

	for (tuple = ISC_LIST_HEAD(temp_diff.tuples);
	     tuple != NULL; tuple = next) {

		INSIST(ttl_good);

		next = ISC_LIST_NEXT(tuple, link);
		/*
		 * See if we already have a REMOVE request in progress.
		 */
		dns_nsec3param_toprivate(&tuple->rdata, &rdata, privatetype,
					 buf, sizeof(buf));

		buf[2] |= DNS_NSEC3FLAG_REMOVE | DNS_NSEC3FLAG_NONSEC;

		CHECK(rr_exists(db, ver, name, &rdata, &flag));
		if (!flag) {
			buf[2] &= ~DNS_NSEC3FLAG_NONSEC;
			CHECK(rr_exists(db, ver, name, &rdata, &flag));
		}

		if (!flag) {
			CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_ADD,
						   name, 0, &rdata, &newtuple));
			CHECK(do_one_tuple(&newtuple, db, ver, diff));
		}
		CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_ADD, name,
					   ttl, &tuple->rdata, &newtuple));
		CHECK(do_one_tuple(&newtuple, db, ver, diff));
		ISC_LIST_UNLINK(temp_diff.tuples, tuple, link);
		dns_diff_appendminimal(diff, &tuple);
		dns_rdata_reset(&rdata);
	}

	result = ISC_R_SUCCESS;
 failure:
	dns_diff_clear(&temp_diff);
	return (result);
}

static isc_result_t
rollback_private(dns_db_t *db, dns_rdatatype_t privatetype,
		 dns_dbversion_t *ver, dns_diff_t *diff)
{
	dns_diff_t temp_diff;
	dns_diffop_t op;
	dns_difftuple_t *tuple, *newtuple = NULL, *next;
	dns_name_t *name = dns_db_origin(db);
	isc_mem_t *mctx = diff->mctx;
	isc_result_t result;

	if (privatetype == 0)
		return (ISC_R_SUCCESS);

	dns_diff_init(mctx, &temp_diff);

	/*
	 * Extract the changes to be rolled back.
	 */
	for (tuple = ISC_LIST_HEAD(diff->tuples);
	     tuple != NULL; tuple = next) {

		next = ISC_LIST_NEXT(tuple, link);

		if (tuple->rdata.type != privatetype ||
		    !dns_name_equal(name, &tuple->name))
			continue;

		/*
		 * Allow records which indicate that a zone has been
		 * signed with a DNSKEY to be removed.
		 */
		if (tuple->op == DNS_DIFFOP_DEL &&
		    tuple->rdata.length == 5 &&
		    tuple->rdata.data[0] != 0 &&
		    tuple->rdata.data[4] != 0)
			continue;

		ISC_LIST_UNLINK(diff->tuples, tuple, link);
		ISC_LIST_PREPEND(temp_diff.tuples, tuple, link);
	}

	/*
	 * Rollback the changes.
	 */
	while ((tuple = ISC_LIST_HEAD(temp_diff.tuples)) != NULL) {
		op = (tuple->op == DNS_DIFFOP_DEL) ?
		      DNS_DIFFOP_ADD : DNS_DIFFOP_DEL;
		CHECK(dns_difftuple_create(mctx, op, name, tuple->ttl,
					   &tuple->rdata, &newtuple));
		CHECK(do_one_tuple(&newtuple, db, ver, &temp_diff));
	}
	result = ISC_R_SUCCESS;

 failure:
	dns_diff_clear(&temp_diff);
	return (result);
}

/*
 * Add records to cause the delayed signing of the zone by added DNSKEY
 * to remove the RRSIG records generated by a deleted DNSKEY.
 */
static isc_result_t
add_signing_records(dns_db_t *db, dns_rdatatype_t privatetype,
		    dns_dbversion_t *ver, dns_diff_t *diff)
{
	dns_difftuple_t *tuple, *newtuple = NULL, *next;
	dns_rdata_dnskey_t dnskey;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_boolean_t flag;
	isc_region_t r;
	isc_result_t result = ISC_R_SUCCESS;
	isc_uint16_t keyid;
	unsigned char buf[5];
	dns_name_t *name = dns_db_origin(db);
	dns_diff_t temp_diff;

	dns_diff_init(diff->mctx, &temp_diff);

	/*
	 * Extract the DNSKEY tuples from the list.
	 */
	for (tuple = ISC_LIST_HEAD(diff->tuples);
	     tuple != NULL; tuple = next) {

		next = ISC_LIST_NEXT(tuple, link);

		if (tuple->rdata.type != dns_rdatatype_dnskey)
			continue;

		ISC_LIST_UNLINK(diff->tuples, tuple, link);
		ISC_LIST_APPEND(temp_diff.tuples, tuple, link);
	}

	/*
	 * Extract TTL changes pairs, we don't need signing records for these.
	 */
	for (tuple = ISC_LIST_HEAD(temp_diff.tuples);
	     tuple != NULL; tuple = next) {
		if (tuple->op == DNS_DIFFOP_ADD) {
			/*
			 * Walk the temp_diff list looking for the
			 * corresponding delete.
			 */
			next = ISC_LIST_HEAD(temp_diff.tuples);
			while (next != NULL) {
				unsigned char *next_data = next->rdata.data;
				unsigned char *tuple_data = tuple->rdata.data;
				if (next->op == DNS_DIFFOP_DEL &&
				    dns_name_equal(&tuple->name, &next->name) &&
				    next->rdata.length == tuple->rdata.length &&
				    !memcmp(next_data, tuple_data,
					    next->rdata.length)) {
					ISC_LIST_UNLINK(temp_diff.tuples, next,
							link);
					ISC_LIST_APPEND(diff->tuples, next,
							link);
					break;
				}
				next = ISC_LIST_NEXT(next, link);
			}
			/*
			 * If we have not found a pair move onto the next
			 * tuple.
			 */
			if (next == NULL) {
				next = ISC_LIST_NEXT(tuple, link);
				continue;
			}
			/*
			 * Find the next tuple to be processed before
			 * unlinking then complete moving the pair to 'diff'.
			 */
			next = ISC_LIST_NEXT(tuple, link);
			ISC_LIST_UNLINK(temp_diff.tuples, tuple, link);
			ISC_LIST_APPEND(diff->tuples, tuple, link);
		} else
			next = ISC_LIST_NEXT(tuple, link);
	}

	/*
	 * Process the remaining DNSKEY entries.
	 */
	for (tuple = ISC_LIST_HEAD(temp_diff.tuples);
	     tuple != NULL;
	     tuple = ISC_LIST_HEAD(temp_diff.tuples)) {

		ISC_LIST_UNLINK(temp_diff.tuples, tuple, link);
		ISC_LIST_APPEND(diff->tuples, tuple, link);

		result = dns_rdata_tostruct(&tuple->rdata, &dnskey, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		if ((dnskey.flags &
		     (DNS_KEYFLAG_OWNERMASK|DNS_KEYTYPE_NOAUTH))
			 != DNS_KEYOWNER_ZONE)
			continue;

		dns_rdata_toregion(&tuple->rdata, &r);

		keyid = dst_region_computeid(&r, dnskey.algorithm);

		buf[0] = dnskey.algorithm;
		buf[1] = (keyid & 0xff00) >> 8;
		buf[2] = (keyid & 0xff);
		buf[3] = (tuple->op == DNS_DIFFOP_ADD) ? 0 : 1;
		buf[4] = 0;
		rdata.data = buf;
		rdata.length = sizeof(buf);
		rdata.type = privatetype;
		rdata.rdclass = tuple->rdata.rdclass;

		CHECK(rr_exists(db, ver, name, &rdata, &flag));
		if (flag)
			continue;
		CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_ADD,
					   name, 0, &rdata, &newtuple));
		CHECK(do_one_tuple(&newtuple, db, ver, diff));
		INSIST(newtuple == NULL);
		/*
		 * Remove any record which says this operation has already
		 * completed.
		 */
		buf[4] = 1;
		CHECK(rr_exists(db, ver, name, &rdata, &flag));
		if (flag) {
			CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_DEL,
						   name, 0, &rdata, &newtuple));
			CHECK(do_one_tuple(&newtuple, db, ver, diff));
			INSIST(newtuple == NULL);
		}
	}

 failure:
	dns_diff_clear(&temp_diff);
	return (result);
}

static isc_boolean_t
isdnssec(dns_db_t *db, dns_dbversion_t *ver, dns_rdatatype_t privatetype) {
	isc_result_t result;
	isc_boolean_t build_nsec, build_nsec3;

	if (dns_db_issecure(db))
		return (ISC_TRUE);

	result = dns_private_chains(db, ver, privatetype,
				    &build_nsec, &build_nsec3);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	return (build_nsec || build_nsec3);
}

static void
update_action(isc_task_t *task, isc_event_t *event) {
	update_event_t *uev = (update_event_t *) event;
	dns_zone_t *zone = uev->zone;
	ns_client_t *client = (ns_client_t *)event->ev_arg;

	isc_result_t result;
	dns_db_t *db = NULL;
	dns_dbversion_t *oldver = NULL;
	dns_dbversion_t *ver = NULL;
	dns_diff_t diff;	/* Pending updates. */
	dns_diff_t temp;	/* Pending RR existence assertions. */
	isc_boolean_t soa_serial_changed = ISC_FALSE;
	isc_mem_t *mctx = client->mctx;
	dns_rdatatype_t covers;
	dns_message_t *request = client->message;
	dns_rdataclass_t zoneclass;
	dns_name_t *zonename;
	dns_ssutable_t *ssutable = NULL;
	dns_fixedname_t tmpnamefixed;
	dns_name_t *tmpname = NULL;
	unsigned int options;
	dns_difftuple_t *tuple;
	dns_rdata_dnskey_t dnskey;
	isc_boolean_t had_dnskey;
	dns_rdatatype_t privatetype = dns_zone_getprivatetype(zone);

	INSIST(event->ev_type == DNS_EVENT_UPDATE);

	dns_diff_init(mctx, &diff);
	dns_diff_init(mctx, &temp);

	CHECK(dns_zone_getdb(zone, &db));
	zonename = dns_db_origin(db);
	zoneclass = dns_db_class(db);
	dns_zone_getssutable(zone, &ssutable);

	/*
	 * Update message processing can leak record existance information
	 * so check that we are allowed to query this zone.  Additionally
	 * if we would refuse all updates for this zone we bail out here.
	 */
	CHECK(checkqueryacl(client, dns_zone_getqueryacl(zone), zonename,
			    dns_zone_getupdateacl(zone), ssutable));

	/*
	 * Get old and new versions now that queryacl has been checked.
	 */
	dns_db_currentversion(db, &oldver);
	CHECK(dns_db_newversion(db, &ver));

	/*
	 * Check prerequisites.
	 */

	for (result = dns_message_firstname(request, DNS_SECTION_PREREQUISITE);
	     result == ISC_R_SUCCESS;
	     result = dns_message_nextname(request, DNS_SECTION_PREREQUISITE))
	{
		dns_name_t *name = NULL;
		dns_rdata_t rdata = DNS_RDATA_INIT;
		dns_ttl_t ttl;
		dns_rdataclass_t update_class;
		isc_boolean_t flag;

		get_current_rr(request, DNS_SECTION_PREREQUISITE, zoneclass,
			       &name, &rdata, &covers, &ttl, &update_class);

		if (ttl != 0)
			PREREQFAILC(DNS_R_FORMERR,
				    "prerequisite TTL is not zero");

		if (! dns_name_issubdomain(name, zonename))
			PREREQFAILN(DNS_R_NOTZONE, name,
				    "prerequisite name is out of zone");

		if (update_class == dns_rdataclass_any) {
			if (rdata.length != 0)
				PREREQFAILC(DNS_R_FORMERR,
				      "class ANY prerequisite "
				      "RDATA is not empty");
			if (rdata.type == dns_rdatatype_any) {
				CHECK(name_exists(db, ver, name, &flag));
				if (! flag) {
					PREREQFAILN(DNS_R_NXDOMAIN, name,
						    "'name in use' "
						    "prerequisite not "
						    "satisfied");
				}
			} else {
				CHECK(rrset_exists(db, ver, name,
						   rdata.type, covers, &flag));
				if (! flag) {
					/* RRset does not exist. */
					PREREQFAILNT(DNS_R_NXRRSET, name, rdata.type,
					"'rrset exists (value independent)' "
					"prerequisite not satisfied");
				}
			}
		} else if (update_class == dns_rdataclass_none) {
			if (rdata.length != 0)
				PREREQFAILC(DNS_R_FORMERR,
					    "class NONE prerequisite "
					    "RDATA is not empty");
			if (rdata.type == dns_rdatatype_any) {
				CHECK(name_exists(db, ver, name, &flag));
				if (flag) {
					PREREQFAILN(DNS_R_YXDOMAIN, name,
						    "'name not in use' "
						    "prerequisite not "
						    "satisfied");
				}
			} else {
				CHECK(rrset_exists(db, ver, name,
						   rdata.type, covers, &flag));
				if (flag) {
					/* RRset exists. */
					PREREQFAILNT(DNS_R_YXRRSET, name,
						     rdata.type,
						     "'rrset does not exist' "
						     "prerequisite not "
						     "satisfied");
				}
			}
		} else if (update_class == zoneclass) {
			/* "temp<rr.name, rr.type> += rr;" */
			result = temp_append(&temp, name, &rdata);
			if (result != ISC_R_SUCCESS) {
				UNEXPECTED_ERROR(__FILE__, __LINE__,
					 "temp entry creation failed: %s",
						 dns_result_totext(result));
				FAIL(ISC_R_UNEXPECTED);
			}
		} else {
			PREREQFAILC(DNS_R_FORMERR, "malformed prerequisite");
		}
	}
	if (result != ISC_R_NOMORE)
		FAIL(result);

	/*
	 * Perform the final check of the "rrset exists (value dependent)"
	 * prerequisites.
	 */
	if (ISC_LIST_HEAD(temp.tuples) != NULL) {
		dns_rdatatype_t type;

		/*
		 * Sort the prerequisite records by owner name,
		 * type, and rdata.
		 */
		result = dns_diff_sort(&temp, temp_order);
		if (result != ISC_R_SUCCESS)
			FAILC(result, "'RRset exists (value dependent)' "
			      "prerequisite not satisfied");

		dns_fixedname_init(&tmpnamefixed);
		tmpname = dns_fixedname_name(&tmpnamefixed);
		result = temp_check(mctx, &temp, db, ver, tmpname, &type);
		if (result != ISC_R_SUCCESS)
			FAILNT(result, tmpname, type,
			       "'RRset exists (value dependent)' "
			       "prerequisite not satisfied");
	}

	update_log(client, zone, LOGLEVEL_DEBUG,
		   "prerequisites are OK");

	/*
	 * Check Requestor's Permissions.  It seems a bit silly to do this
	 * only after prerequisite testing, but that is what RFC2136 says.
	 */
	if (ssutable == NULL)
		CHECK(checkupdateacl(client, dns_zone_getupdateacl(zone),
				     "update", zonename, ISC_FALSE, ISC_FALSE));
	else if (client->signer == NULL && !TCPCLIENT(client))
		CHECK(checkupdateacl(client, NULL, "update", zonename,
				     ISC_FALSE, ISC_TRUE));

	if (dns_zone_getupdatedisabled(zone))
		FAILC(DNS_R_REFUSED, "dynamic update temporarily disabled "
				     "because the zone is frozen.  Use "
				     "'rndc thaw' to re-enable updates.");

	/*
	 * Perform the Update Section Prescan.
	 */

	for (result = dns_message_firstname(request, DNS_SECTION_UPDATE);
	     result == ISC_R_SUCCESS;
	     result = dns_message_nextname(request, DNS_SECTION_UPDATE))
	{
		dns_name_t *name = NULL;
		dns_rdata_t rdata = DNS_RDATA_INIT;
		dns_ttl_t ttl;
		dns_rdataclass_t update_class;
		get_current_rr(request, DNS_SECTION_UPDATE, zoneclass,
			       &name, &rdata, &covers, &ttl, &update_class);

		if (! dns_name_issubdomain(name, zonename))
			FAILC(DNS_R_NOTZONE,
			      "update RR is outside zone");
		if (update_class == zoneclass) {
			/*
			 * Check for meta-RRs.  The RFC2136 pseudocode says
			 * check for ANY|AXFR|MAILA|MAILB, but the text adds
			 * "or any other QUERY metatype"
			 */
			if (dns_rdatatype_ismeta(rdata.type)) {
				FAILC(DNS_R_FORMERR,
				      "meta-RR in update");
			}
			result = dns_zone_checknames(zone, name, &rdata);
			if (result != ISC_R_SUCCESS)
				FAIL(DNS_R_REFUSED);
		} else if (update_class == dns_rdataclass_any) {
			if (ttl != 0 || rdata.length != 0 ||
			    (dns_rdatatype_ismeta(rdata.type) &&
			     rdata.type != dns_rdatatype_any))
				FAILC(DNS_R_FORMERR,
				      "meta-RR in update");
		} else if (update_class == dns_rdataclass_none) {
			if (ttl != 0 ||
			    dns_rdatatype_ismeta(rdata.type))
				FAILC(DNS_R_FORMERR,
				      "meta-RR in update");
		} else {
			update_log(client, zone, ISC_LOG_WARNING,
				   "update RR has incorrect class %d",
				   update_class);
			FAIL(DNS_R_FORMERR);
		}

		/*
		 * draft-ietf-dnsind-simple-secure-update-01 says
		 * "Unlike traditional dynamic update, the client
		 * is forbidden from updating NSEC records."
		 */
		if (rdata.type == dns_rdatatype_nsec3) {
			FAILC(DNS_R_REFUSED,
			      "explicit NSEC3 updates are not allowed "
			      "in secure zones");
		} else if (rdata.type == dns_rdatatype_nsec) {
			FAILC(DNS_R_REFUSED,
			      "explicit NSEC updates are not allowed "
			      "in secure zones");
		} else if (rdata.type == dns_rdatatype_rrsig &&
			   !dns_name_equal(name, zonename)) {
			FAILC(DNS_R_REFUSED,
			      "explicit RRSIG updates are currently "
			      "not supported in secure zones except "
			      "at the apex");
		}

		if (ssutable != NULL) {
			isc_netaddr_t *tcpaddr, netaddr;
			dst_key_t *tsigkey = NULL;
			/*
			 * If this is a TCP connection then pass the
			 * address of the client through for tcp-self
			 * and 6to4-self otherwise pass NULL.  This
			 * provides weak address based authentication.
			 */
			if (TCPCLIENT(client)) {
				isc_netaddr_fromsockaddr(&netaddr,
							 &client->peeraddr);
				tcpaddr = &netaddr;
			} else
				tcpaddr = NULL;

			if (client->message->tsigkey != NULL)
				tsigkey = client->message->tsigkey->key;

			if (rdata.type != dns_rdatatype_any) {
				if (!dns_ssutable_checkrules(ssutable,
							     client->signer,
							     name, tcpaddr,
							     rdata.type,
							     tsigkey))
					FAILC(DNS_R_REFUSED,
					      "rejected by secure update");
			} else {
				if (!ssu_checkall(db, ver, name, ssutable,
						  client->signer, tcpaddr,
						  tsigkey))
					FAILC(DNS_R_REFUSED,
					      "rejected by secure update");
			}
		}
	}
	if (result != ISC_R_NOMORE)
		FAIL(result);

	update_log(client, zone, LOGLEVEL_DEBUG,
		   "update section prescan OK");

	/*
	 * Process the Update Section.
	 */

	options = dns_zone_getoptions(zone);
	for (result = dns_message_firstname(request, DNS_SECTION_UPDATE);
	     result == ISC_R_SUCCESS;
	     result = dns_message_nextname(request, DNS_SECTION_UPDATE))
	{
		dns_name_t *name = NULL;
		dns_rdata_t rdata = DNS_RDATA_INIT;
		dns_ttl_t ttl;
		dns_rdataclass_t update_class;
		isc_boolean_t flag;

		get_current_rr(request, DNS_SECTION_UPDATE, zoneclass,
			       &name, &rdata, &covers, &ttl, &update_class);

		if (update_class == zoneclass) {

			/*
			 * RFC1123 doesn't allow MF and MD in master zones.				 */
			if (rdata.type == dns_rdatatype_md ||
			    rdata.type == dns_rdatatype_mf) {
				char typebuf[DNS_RDATATYPE_FORMATSIZE];

				dns_rdatatype_format(rdata.type, typebuf,
						     sizeof(typebuf));
				update_log(client, zone, LOGLEVEL_PROTOCOL,
					   "attempt to add %s ignored",
					   typebuf);
				continue;
			}
			if ((rdata.type == dns_rdatatype_ns ||
			     rdata.type == dns_rdatatype_dname) &&
			    dns_name_iswildcard(name)) {
				char typebuf[DNS_RDATATYPE_FORMATSIZE];

				dns_rdatatype_format(rdata.type, typebuf,
						     sizeof(typebuf));
				update_log(client, zone,
					   LOGLEVEL_PROTOCOL,
					   "attempt to add wildcard %s record "
					   "ignored", typebuf);
				continue;
			}
			if (rdata.type == dns_rdatatype_cname) {
				CHECK(cname_incompatible_rrset_exists(db, ver,
								      name,
								      &flag));
				if (flag) {
					update_log(client, zone,
						   LOGLEVEL_PROTOCOL,
						   "attempt to add CNAME "
						   "alongside non-CNAME "
						   "ignored");
					continue;
				}
			} else {
				CHECK(rrset_exists(db, ver, name,
						   dns_rdatatype_cname, 0,
						   &flag));
				if (flag &&
				    ! dns_rdatatype_isdnssec(rdata.type))
				{
					update_log(client, zone,
						   LOGLEVEL_PROTOCOL,
						   "attempt to add non-CNAME "
						   "alongside CNAME ignored");
					continue;
				}
			}
			if (rdata.type == dns_rdatatype_soa) {
				isc_boolean_t ok;
				CHECK(rrset_exists(db, ver, name,
						   dns_rdatatype_soa, 0,
						   &flag));
				if (! flag) {
					update_log(client, zone,
						   LOGLEVEL_PROTOCOL,
						   "attempt to create 2nd "
						   "SOA ignored");
					continue;
				}
				CHECK(check_soa_increment(db, ver, &rdata,
							  &ok));
				if (! ok) {
					update_log(client, zone,
						   LOGLEVEL_PROTOCOL,
						   "SOA update failed to "
						   "increment serial, "
						   "ignoring it");
					continue;
				}
				soa_serial_changed = ISC_TRUE;
			}

			if (rdata.type == privatetype) {
				update_log(client, zone, LOGLEVEL_PROTOCOL,
					   "attempt to add a private type "
					   "(%u) record rejected internal "
					   "use only", privatetype);
				continue;
			}

			if (rdata.type == dns_rdatatype_nsec3param) {
				/*
				 * Ignore attempts to add NSEC3PARAM records
				 * with any flags other than OPTOUT.
				 */
				if ((rdata.data[1] & ~DNS_NSEC3FLAG_OPTOUT) != 0) {
					update_log(client, zone,
						   LOGLEVEL_PROTOCOL,
						   "attempt to add NSEC3PARAM "
						   "record with non OPTOUT "
						   "flag");
					continue;
				}
			}

			if ((options & DNS_ZONEOPT_CHECKWILDCARD) != 0 &&
			    dns_name_internalwildcard(name)) {
				char namestr[DNS_NAME_FORMATSIZE];
				dns_name_format(name, namestr,
						sizeof(namestr));
				update_log(client, zone, LOGLEVEL_PROTOCOL,
					   "warning: ownername '%s' contains "
					   "a non-terminal wildcard", namestr);
			}

			if (isc_log_wouldlog(ns_g_lctx, LOGLEVEL_PROTOCOL)) {
				char namestr[DNS_NAME_FORMATSIZE];
				char typestr[DNS_RDATATYPE_FORMATSIZE];
				dns_name_format(name, namestr,
						sizeof(namestr));
				dns_rdatatype_format(rdata.type, typestr,
						     sizeof(typestr));
				update_log(client, zone, LOGLEVEL_PROTOCOL,
					   "adding an RR at '%s' %s",
					   namestr, typestr);
			}

			/* Prepare the affected RRset for the addition. */
			{
				add_rr_prepare_ctx_t ctx;
				ctx.db = db;
				ctx.ver = ver;
				ctx.diff = &diff;
				ctx.name = name;
				ctx.update_rr = &rdata;
				ctx.update_rr_ttl = ttl;
				ctx.ignore_add = ISC_FALSE;
				dns_diff_init(mctx, &ctx.del_diff);
				dns_diff_init(mctx, &ctx.add_diff);
				CHECK(foreach_rr(db, ver, name, rdata.type,
						 covers, add_rr_prepare_action,
						 &ctx));

				if (ctx.ignore_add) {
					dns_diff_clear(&ctx.del_diff);
					dns_diff_clear(&ctx.add_diff);
				} else {
					result = do_diff(&ctx.del_diff, db, ver,
							 &diff);
					if (result == ISC_R_SUCCESS) {
						result = do_diff(&ctx.add_diff,
								 db, ver,
								 &diff);
					}
					if (result != ISC_R_SUCCESS) {
						dns_diff_clear(&ctx.del_diff);
						dns_diff_clear(&ctx.add_diff);
						goto failure;
					}
					CHECK(update_one_rr(db, ver, &diff,
							    DNS_DIFFOP_ADD,
							    name, ttl, &rdata));
				}
			}
		} else if (update_class == dns_rdataclass_any) {
			if (rdata.type == dns_rdatatype_any) {
				if (isc_log_wouldlog(ns_g_lctx,
						     LOGLEVEL_PROTOCOL))
				{
					char namestr[DNS_NAME_FORMATSIZE];
					dns_name_format(name, namestr,
							sizeof(namestr));
					update_log(client, zone,
						   LOGLEVEL_PROTOCOL,
						   "delete all rrsets from "
						   "name '%s'", namestr);
				}
				if (dns_name_equal(name, zonename)) {
					CHECK(delete_if(type_not_soa_nor_ns_p,
							db, ver, name,
							dns_rdatatype_any, 0,
							&rdata, &diff));
				} else {
					CHECK(delete_if(type_not_dnssec,
							db, ver, name,
							dns_rdatatype_any, 0,
							&rdata, &diff));
				}
			} else if (dns_name_equal(name, zonename) &&
				   (rdata.type == dns_rdatatype_soa ||
				    rdata.type == dns_rdatatype_ns)) {
				update_log(client, zone, LOGLEVEL_PROTOCOL,
					   "attempt to delete all SOA "
					   "or NS records ignored");
				continue;
			} else {
				if (isc_log_wouldlog(ns_g_lctx,
						     LOGLEVEL_PROTOCOL))
				{
					char namestr[DNS_NAME_FORMATSIZE];
					char typestr[DNS_RDATATYPE_FORMATSIZE];
					dns_name_format(name, namestr,
							sizeof(namestr));
					dns_rdatatype_format(rdata.type,
							     typestr,
							     sizeof(typestr));
					update_log(client, zone,
						   LOGLEVEL_PROTOCOL,
						   "deleting rrset at '%s' %s",
						   namestr, typestr);
				}
				CHECK(delete_if(true_p, db, ver, name,
						rdata.type, covers, &rdata,
						&diff));
			}
		} else if (update_class == dns_rdataclass_none) {
			char namestr[DNS_NAME_FORMATSIZE];
			char typestr[DNS_RDATATYPE_FORMATSIZE];

			/*
			 * The (name == zonename) condition appears in
			 * RFC2136 3.4.2.4 but is missing from the pseudocode.
			 */
			if (dns_name_equal(name, zonename)) {
				if (rdata.type == dns_rdatatype_soa) {
					update_log(client, zone,
						   LOGLEVEL_PROTOCOL,
						   "attempt to delete SOA "
						   "ignored");
					continue;
				}
				if (rdata.type == dns_rdatatype_ns) {
					int count;
					CHECK(rr_count(db, ver, name,
						       dns_rdatatype_ns,
						       0, &count));
					if (count == 1) {
						update_log(client, zone,
							   LOGLEVEL_PROTOCOL,
							   "attempt to "
							   "delete last "
							   "NS ignored");
						continue;
					}
				}
			}
			dns_name_format(name, namestr, sizeof(namestr));
			dns_rdatatype_format(rdata.type, typestr,
					     sizeof(typestr));
			update_log(client, zone, LOGLEVEL_PROTOCOL,
				   "deleting an RR at %s %s", namestr, typestr);
			CHECK(delete_if(rr_equal_p, db, ver, name, rdata.type,
					covers, &rdata, &diff));
		}
	}
	if (result != ISC_R_NOMORE)
		FAIL(result);

	/*
	 * Check that any changes to DNSKEY/NSEC3PARAM records make sense.
	 * If they don't then back out all changes to DNSKEY/NSEC3PARAM
	 * records.
	 */
	if (! ISC_LIST_EMPTY(diff.tuples))
		CHECK(check_dnssec(client, zone, db, ver, &diff));

	if (! ISC_LIST_EMPTY(diff.tuples)) {
		unsigned int errors = 0;
		CHECK(dns_zone_nscheck(zone, db, ver, &errors));
		if (errors != 0) {
			update_log(client, zone, LOGLEVEL_PROTOCOL,
				   "update rejected: post update name server "
				   "sanity check failed");
			result = DNS_R_REFUSED;
			goto failure;
		}
	}
	if (! ISC_LIST_EMPTY(diff.tuples)) {
		result = dns_zone_cdscheck(zone, db, ver);
		if (result == DNS_R_BADCDS || result == DNS_R_BADCDNSKEY) {
			update_log(client, zone, LOGLEVEL_PROTOCOL,
				   "update rejected: bad %s RRset",
				   result == DNS_R_BADCDS ? "CDS" : "CDNSKEY");
			result = DNS_R_REFUSED;
			goto failure;
		}
		if (result != ISC_R_SUCCESS)
			goto failure;

	}

	/*
	 * If any changes were made, increment the SOA serial number,
	 * update RRSIGs and NSECs (if zone is secure), and write the update
	 * to the journal.
	 */
	if (! ISC_LIST_EMPTY(diff.tuples)) {
		char *journalfile;
		dns_journal_t *journal;
		isc_boolean_t has_dnskey;

		/*
		 * Increment the SOA serial, but only if it was not
		 * changed as a result of an update operation.
		 */
		if (! soa_serial_changed) {
			CHECK(update_soa_serial(db, ver, &diff, mctx,
				       dns_zone_getserialupdatemethod(zone)));
		}

		CHECK(check_mx(client, zone, db, ver, &diff));

		CHECK(remove_orphaned_ds(db, ver, &diff));

		CHECK(rrset_exists(db, ver, zonename, dns_rdatatype_dnskey,
				   0, &has_dnskey));

#define ALLOW_SECURE_TO_INSECURE(zone) \
	((dns_zone_getoptions(zone) & DNS_ZONEOPT_SECURETOINSECURE) != 0)

		CHECK(rrset_exists(db, oldver, zonename, dns_rdatatype_dnskey,
				   0, &had_dnskey));
		if (!ALLOW_SECURE_TO_INSECURE(zone)) {
			if (had_dnskey && !has_dnskey) {
				update_log(client, zone, LOGLEVEL_PROTOCOL,
					   "update rejected: all DNSKEY "
					   "records removed and "
					   "'dnssec-secure-to-insecure' "
					   "not set");
				result = DNS_R_REFUSED;
				goto failure;
			}
		}

		CHECK(rollback_private(db, privatetype, ver, &diff));

		CHECK(add_signing_records(db, privatetype, ver, &diff));

		CHECK(add_nsec3param_records(client, zone, db, ver, &diff));

		if (had_dnskey && !has_dnskey) {
			/*
			 * We are transitioning from secure to insecure.
			 * Cause all NSEC3 chains to be deleted.  When the
			 * the last signature for the DNSKEY records are
			 * remove any NSEC chain present will also be removed.
			 */
			 CHECK(dns_nsec3param_deletechains(db, ver, zone,
							   ISC_TRUE, &diff));
		} else if (has_dnskey && isdnssec(db, ver, privatetype)) {
			isc_uint32_t interval;
			dns_update_log_t log;

			interval = dns_zone_getsigvalidityinterval(zone);
			log.func = update_log_cb;
			log.arg = client;
			result = dns_update_signatures(&log, zone, db, oldver,
						       ver, &diff, interval);

			if (result != ISC_R_SUCCESS) {
				update_log(client, zone,
					   ISC_LOG_ERROR,
					   "RRSIG/NSEC/NSEC3 update failed: %s",
					   isc_result_totext(result));
				goto failure;
			}
		}

		journalfile = dns_zone_getjournal(zone);
		if (journalfile != NULL) {
			update_log(client, zone, LOGLEVEL_DEBUG,
				   "writing journal %s", journalfile);

			journal = NULL;
			result = dns_journal_open(mctx, journalfile,
						  DNS_JOURNAL_CREATE, &journal);
			if (result != ISC_R_SUCCESS)
				FAILS(result, "journal open failed");

			result = dns_journal_write_transaction(journal, &diff);
			if (result != ISC_R_SUCCESS) {
				dns_journal_destroy(&journal);
				FAILS(result, "journal write failed");
			}

			dns_journal_destroy(&journal);
		}

		/*
		 * XXXRTH  Just a note that this committing code will have
		 *	   to change to handle databases that need two-phase
		 *	   commit, but this isn't a priority.
		 */
		update_log(client, zone, LOGLEVEL_DEBUG,
			   "committing update transaction");

		dns_db_closeversion(db, &ver, ISC_TRUE);

		/*
		 * Mark the zone as dirty so that it will be written to disk.
		 */
		dns_zone_markdirty(zone);

		/*
		 * Notify slaves of the change we just made.
		 */
		dns_zone_notify(zone);

		/*
		 * Cause the zone to be signed with the key that we
		 * have just added or have the corresponding signatures
		 * deleted.
		 *
		 * Note: we are already committed to this course of action.
		 */
		for (tuple = ISC_LIST_HEAD(diff.tuples);
		     tuple != NULL;
		     tuple = ISC_LIST_NEXT(tuple, link)) {
			isc_region_t r;
			dns_secalg_t algorithm;
			isc_uint16_t keyid;

			if (tuple->rdata.type != dns_rdatatype_dnskey)
				continue;

			dns_rdata_tostruct(&tuple->rdata, &dnskey, NULL);
			if ((dnskey.flags &
			     (DNS_KEYFLAG_OWNERMASK|DNS_KEYTYPE_NOAUTH))
				 != DNS_KEYOWNER_ZONE)
				continue;

			dns_rdata_toregion(&tuple->rdata, &r);
			algorithm = dnskey.algorithm;
			keyid = dst_region_computeid(&r, algorithm);

			result = dns_zone_signwithkey(zone, algorithm, keyid,
					ISC_TF(tuple->op == DNS_DIFFOP_DEL));
			if (result != ISC_R_SUCCESS) {
				update_log(client, zone, ISC_LOG_ERROR,
					   "dns_zone_signwithkey failed: %s",
					   dns_result_totext(result));
			}
		}

		/*
		 * Cause the zone to add/delete NSEC3 chains for the
		 * deferred NSEC3PARAM changes.
		 *
		 * Note: we are already committed to this course of action.
		 */
		for (tuple = ISC_LIST_HEAD(diff.tuples);
		     tuple != NULL;
		     tuple = ISC_LIST_NEXT(tuple, link)) {
			unsigned char buf[DNS_NSEC3PARAM_BUFFERSIZE];
			dns_rdata_t rdata = DNS_RDATA_INIT;
			dns_rdata_nsec3param_t nsec3param;

			if (tuple->rdata.type != privatetype ||
			    tuple->op != DNS_DIFFOP_ADD)
				continue;

			if (!dns_nsec3param_fromprivate(&tuple->rdata, &rdata,
						   buf, sizeof(buf)))
				continue;
			dns_rdata_tostruct(&rdata, &nsec3param, NULL);
			if (nsec3param.flags == 0)
				continue;

			result = dns_zone_addnsec3chain(zone, &nsec3param);
			if (result != ISC_R_SUCCESS) {
				update_log(client, zone, ISC_LOG_ERROR,
					   "dns_zone_addnsec3chain failed: %s",
					   dns_result_totext(result));
			}
		}
	} else {
		update_log(client, zone, LOGLEVEL_DEBUG, "redundant request");
		dns_db_closeversion(db, &ver, ISC_TRUE);
	}
	result = ISC_R_SUCCESS;
	goto common;

 failure:
	/*
	 * The reason for failure should have been logged at this point.
	 */
	if (ver != NULL) {
		update_log(client, zone, LOGLEVEL_DEBUG,
			   "rolling back");
		dns_db_closeversion(db, &ver, ISC_FALSE);
	}

 common:
	dns_diff_clear(&temp);
	dns_diff_clear(&diff);

	if (oldver != NULL)
		dns_db_closeversion(db, &oldver, ISC_FALSE);

	if (db != NULL)
		dns_db_detach(&db);

	if (ssutable != NULL)
		dns_ssutable_detach(&ssutable);

	isc_task_detach(&task);
	uev->result = result;
	if (zone != NULL)
		INSIST(uev->zone == zone); /* we use this later */
	uev->ev_type = DNS_EVENT_UPDATEDONE;
	uev->ev_action = updatedone_action;
	isc_task_send(client->task, &event);

	INSIST(ver == NULL);
	INSIST(event == NULL);
}

static void
updatedone_action(isc_task_t *task, isc_event_t *event) {
	update_event_t *uev = (update_event_t *) event;
	ns_client_t *client = (ns_client_t *) event->ev_arg;

	UNUSED(task);

	INSIST(event->ev_type == DNS_EVENT_UPDATEDONE);
	INSIST(task == client->task);

	INSIST(client->nupdates > 0);
	switch (uev->result) {
	case ISC_R_SUCCESS:
		inc_stats(uev->zone, dns_nsstatscounter_updatedone);
		break;
	case DNS_R_REFUSED:
		inc_stats(uev->zone, dns_nsstatscounter_updaterej);
		break;
	default:
		inc_stats(uev->zone, dns_nsstatscounter_updatefail);
		break;
	}
	if (uev->zone != NULL)
		dns_zone_detach(&uev->zone);
	client->nupdates--;
	respond(client, uev->result);
	isc_event_free(&event);
	ns_client_detach(&client);
}

/*%
 * Update forwarding support.
 */

static void
forward_fail(isc_task_t *task, isc_event_t *event) {
	ns_client_t *client = (ns_client_t *)event->ev_arg;

	UNUSED(task);

	INSIST(client->nupdates > 0);
	client->nupdates--;
	respond(client, DNS_R_SERVFAIL);
	isc_event_free(&event);
	ns_client_detach(&client);
}


static void
forward_callback(void *arg, isc_result_t result, dns_message_t *answer) {
	update_event_t *uev = arg;
	ns_client_t *client = uev->ev_arg;
	dns_zone_t *zone = uev->zone;

	if (result != ISC_R_SUCCESS) {
		INSIST(answer == NULL);
		uev->ev_type = DNS_EVENT_UPDATEDONE;
		uev->ev_action = forward_fail;
		inc_stats(zone, dns_nsstatscounter_updatefwdfail);
	} else {
		uev->ev_type = DNS_EVENT_UPDATEDONE;
		uev->ev_action = forward_done;
		uev->answer = answer;
		inc_stats(zone, dns_nsstatscounter_updaterespfwd);
	}
	isc_task_send(client->task, ISC_EVENT_PTR(&uev));
	dns_zone_detach(&zone);
}

static void
forward_done(isc_task_t *task, isc_event_t *event) {
	update_event_t *uev = (update_event_t *) event;
	ns_client_t *client = (ns_client_t *)event->ev_arg;

	UNUSED(task);

	INSIST(client->nupdates > 0);
	client->nupdates--;
	ns_client_sendraw(client, uev->answer);
	dns_message_destroy(&uev->answer);
	isc_event_free(&event);
	ns_client_detach(&client);
}

static void
forward_action(isc_task_t *task, isc_event_t *event) {
	update_event_t *uev = (update_event_t *) event;
	dns_zone_t *zone = uev->zone;
	ns_client_t *client = (ns_client_t *)event->ev_arg;
	isc_result_t result;

	result = dns_zone_forwardupdate(zone, client->message,
					forward_callback, event);
	if (result != ISC_R_SUCCESS) {
		uev->ev_type = DNS_EVENT_UPDATEDONE;
		uev->ev_action = forward_fail;
		isc_task_send(client->task, &event);
		inc_stats(zone, dns_nsstatscounter_updatefwdfail);
		dns_zone_detach(&zone);
	} else
		inc_stats(zone, dns_nsstatscounter_updatereqfwd);
	isc_task_detach(&task);
}

static isc_result_t
send_forward_event(ns_client_t *client, dns_zone_t *zone) {
	char namebuf[DNS_NAME_FORMATSIZE];
	char classbuf[DNS_RDATACLASS_FORMATSIZE];
	isc_result_t result = ISC_R_SUCCESS;
	update_event_t *event = NULL;
	isc_task_t *zonetask = NULL;
	ns_client_t *evclient;

	/*
	 * This may take some time so replace this client.
	 */
	if (!client->mortal && (client->attributes & NS_CLIENTATTR_TCP) == 0)
		CHECK(ns_client_replace(client));

	event = (update_event_t *)
		isc_event_allocate(client->mctx, client, DNS_EVENT_UPDATE,
				   forward_action, NULL, sizeof(*event));
	if (event == NULL)
		FAIL(ISC_R_NOMEMORY);
	event->zone = zone;
	event->result = ISC_R_SUCCESS;

	evclient = NULL;
	ns_client_attach(client, &evclient);
	INSIST(client->nupdates == 0);
	client->nupdates++;
	event->ev_arg = evclient;

	dns_name_format(dns_zone_getorigin(zone), namebuf,
			sizeof(namebuf));
	dns_rdataclass_format(dns_zone_getclass(zone), classbuf,
			      sizeof(classbuf));

	ns_client_log(client, NS_LOGCATEGORY_UPDATE, NS_LOGMODULE_UPDATE,
		      LOGLEVEL_PROTOCOL, "forwarding update for zone '%s/%s'",
		      namebuf, classbuf);

	dns_zone_gettask(zone, &zonetask);
	isc_task_send(zonetask, ISC_EVENT_PTR(&event));

 failure:
	if (event != NULL)
		isc_event_free(ISC_EVENT_PTR(&event));
	return (result);
}
