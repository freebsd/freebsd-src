/*
 * Copyright (C) 2004-2010  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: xfrout.c,v 1.131.26.6 2010-05-27 23:48:18 tbox Exp $ */

#include <config.h>

#include <isc/formatcheck.h>
#include <isc/mem.h>
#include <isc/timer.h>
#include <isc/print.h>
#include <isc/stats.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/dbiterator.h>
#ifdef DLZ
#include <dns/dlz.h>
#endif
#include <dns/fixedname.h>
#include <dns/journal.h>
#include <dns/message.h>
#include <dns/peer.h>
#include <dns/rdataclass.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/result.h>
#include <dns/soa.h>
#include <dns/stats.h>
#include <dns/timer.h>
#include <dns/tsig.h>
#include <dns/view.h>
#include <dns/zone.h>
#include <dns/zt.h>

#include <named/client.h>
#include <named/log.h>
#include <named/server.h>
#include <named/xfrout.h>

/*! \file
 * \brief
 * Outgoing AXFR and IXFR.
 */

/*
 * TODO:
 *  - IXFR over UDP
 */

#define XFROUT_COMMON_LOGARGS \
	ns_g_lctx, DNS_LOGCATEGORY_XFER_OUT, NS_LOGMODULE_XFER_OUT

#define XFROUT_PROTOCOL_LOGARGS \
	XFROUT_COMMON_LOGARGS, ISC_LOG_INFO

#define XFROUT_DEBUG_LOGARGS(n) \
	XFROUT_COMMON_LOGARGS, ISC_LOG_DEBUG(n)

#define XFROUT_RR_LOGARGS \
	XFROUT_COMMON_LOGARGS, XFROUT_RR_LOGLEVEL

#define XFROUT_RR_LOGLEVEL	ISC_LOG_DEBUG(8)

/*%
 * Fail unconditionally and log as a client error.
 * The test against ISC_R_SUCCESS is there to keep the Solaris compiler
 * from complaining about "end-of-loop code not reached".
 */
#define FAILC(code, msg) \
	do {							\
		result = (code);				\
		ns_client_log(client, DNS_LOGCATEGORY_XFER_OUT, \
			   NS_LOGMODULE_XFER_OUT, ISC_LOG_INFO, \
			   "bad zone transfer request: %s (%s)", \
			   msg, isc_result_totext(code));	\
		if (result != ISC_R_SUCCESS) goto failure;	\
	} while (0)

#define FAILQ(code, msg, question, rdclass) \
	do {							\
		char _buf1[DNS_NAME_FORMATSIZE];		\
		char _buf2[DNS_RDATACLASS_FORMATSIZE]; 		\
		result = (code);				\
		dns_name_format(question, _buf1, sizeof(_buf1));  \
		dns_rdataclass_format(rdclass, _buf2, sizeof(_buf2)); \
		ns_client_log(client, DNS_LOGCATEGORY_XFER_OUT, \
			   NS_LOGMODULE_XFER_OUT, ISC_LOG_INFO, \
			   "bad zone transfer request: '%s/%s': %s (%s)", \
			   _buf1, _buf2, msg, isc_result_totext(code));	\
		if (result != ISC_R_SUCCESS) goto failure;	\
	} while (0)

#define CHECK(op) \
	do { result = (op); 					\
		if (result != ISC_R_SUCCESS) goto failure; 	\
	} while (0)

/**************************************************************************/
/*%
 * A db_rr_iterator_t is an iterator that iterates over an entire database,
 * returning one RR at a time, in some arbitrary order.
 */

typedef struct db_rr_iterator db_rr_iterator_t;

/*% db_rr_iterator structure */
struct db_rr_iterator {
	isc_result_t		result;
	dns_db_t		*db;
	dns_dbiterator_t 	*dbit;
	dns_dbversion_t 	*ver;
	isc_stdtime_t		now;
	dns_dbnode_t		*node;
	dns_fixedname_t		fixedname;
	dns_rdatasetiter_t 	*rdatasetit;
	dns_rdataset_t 		rdataset;
	dns_rdata_t		rdata;
};

static isc_result_t
db_rr_iterator_init(db_rr_iterator_t *it, dns_db_t *db, dns_dbversion_t *ver,
		    isc_stdtime_t now);

static isc_result_t
db_rr_iterator_first(db_rr_iterator_t *it);

static isc_result_t
db_rr_iterator_next(db_rr_iterator_t *it);

static void
db_rr_iterator_current(db_rr_iterator_t *it, dns_name_t **name,
		       isc_uint32_t *ttl, dns_rdata_t **rdata);

static void
db_rr_iterator_destroy(db_rr_iterator_t *it);

static inline void
inc_stats(dns_zone_t *zone, isc_statscounter_t counter) {
	isc_stats_increment(ns_g_server->nsstats, counter);
	if (zone != NULL) {
		isc_stats_t *zonestats = dns_zone_getrequeststats(zone);
		if (zonestats != NULL)
			isc_stats_increment(zonestats, counter);
	}
}

static isc_result_t
db_rr_iterator_init(db_rr_iterator_t *it, dns_db_t *db, dns_dbversion_t *ver,
		    isc_stdtime_t now)
{
	isc_result_t result;
	it->db = db;
	it->dbit = NULL;
	it->ver = ver;
	it->now = now;
	it->node = NULL;
	result = dns_db_createiterator(it->db, 0, &it->dbit);
	if (result != ISC_R_SUCCESS)
		return (result);
	it->rdatasetit = NULL;
	dns_rdata_init(&it->rdata);
	dns_rdataset_init(&it->rdataset);
	dns_fixedname_init(&it->fixedname);
	INSIST(! dns_rdataset_isassociated(&it->rdataset));
	it->result = ISC_R_SUCCESS;
	return (it->result);
}

static isc_result_t
db_rr_iterator_first(db_rr_iterator_t *it) {
	it->result = dns_dbiterator_first(it->dbit);
	/*
	 * The top node may be empty when out of zone glue exists.
	 * Walk the tree to find the first node with data.
	 */
	while (it->result == ISC_R_SUCCESS) {
		it->result = dns_dbiterator_current(it->dbit, &it->node,
				    dns_fixedname_name(&it->fixedname));
		if (it->result != ISC_R_SUCCESS)
			return (it->result);

		it->result = dns_db_allrdatasets(it->db, it->node,
						 it->ver, it->now,
						 &it->rdatasetit);
		if (it->result != ISC_R_SUCCESS)
			return (it->result);

		it->result = dns_rdatasetiter_first(it->rdatasetit);
		if (it->result != ISC_R_SUCCESS) {
			/*
			 * This node is empty. Try next node.
			 */
			dns_rdatasetiter_destroy(&it->rdatasetit);
			dns_db_detachnode(it->db, &it->node);
			it->result = dns_dbiterator_next(it->dbit);
			continue;
		}
		dns_rdatasetiter_current(it->rdatasetit, &it->rdataset);
		it->rdataset.attributes |= DNS_RDATASETATTR_LOADORDER;
		it->result = dns_rdataset_first(&it->rdataset);
		return (it->result);
	}
	return (it->result);
}


static isc_result_t
db_rr_iterator_next(db_rr_iterator_t *it) {
	if (it->result != ISC_R_SUCCESS)
		return (it->result);

	INSIST(it->dbit != NULL);
	INSIST(it->node != NULL);
	INSIST(it->rdatasetit != NULL);

	it->result = dns_rdataset_next(&it->rdataset);
	if (it->result == ISC_R_NOMORE) {
		dns_rdataset_disassociate(&it->rdataset);
		it->result = dns_rdatasetiter_next(it->rdatasetit);
		/*
		 * The while loop body is executed more than once
		 * only when an empty dbnode needs to be skipped.
		 */
		while (it->result == ISC_R_NOMORE) {
			dns_rdatasetiter_destroy(&it->rdatasetit);
			dns_db_detachnode(it->db, &it->node);
			it->result = dns_dbiterator_next(it->dbit);
			if (it->result == ISC_R_NOMORE) {
				/* We are at the end of the entire database. */
				return (it->result);
			}
			if (it->result != ISC_R_SUCCESS)
				return (it->result);
			it->result = dns_dbiterator_current(it->dbit,
				    &it->node,
				    dns_fixedname_name(&it->fixedname));
			if (it->result != ISC_R_SUCCESS)
				return (it->result);
			it->result = dns_db_allrdatasets(it->db, it->node,
					 it->ver, it->now,
					 &it->rdatasetit);
			if (it->result != ISC_R_SUCCESS)
				return (it->result);
			it->result = dns_rdatasetiter_first(it->rdatasetit);
		}
		if (it->result != ISC_R_SUCCESS)
			return (it->result);
		dns_rdatasetiter_current(it->rdatasetit, &it->rdataset);
		it->rdataset.attributes |= DNS_RDATASETATTR_LOADORDER;
		it->result = dns_rdataset_first(&it->rdataset);
		if (it->result != ISC_R_SUCCESS)
			return (it->result);
	}
	return (it->result);
}

static void
db_rr_iterator_pause(db_rr_iterator_t *it) {
	RUNTIME_CHECK(dns_dbiterator_pause(it->dbit) == ISC_R_SUCCESS);
}

static void
db_rr_iterator_destroy(db_rr_iterator_t *it) {
	if (dns_rdataset_isassociated(&it->rdataset))
		dns_rdataset_disassociate(&it->rdataset);
	if (it->rdatasetit != NULL)
		dns_rdatasetiter_destroy(&it->rdatasetit);
	if (it->node != NULL)
		dns_db_detachnode(it->db, &it->node);
	dns_dbiterator_destroy(&it->dbit);
}

static void
db_rr_iterator_current(db_rr_iterator_t *it, dns_name_t **name,
		      isc_uint32_t *ttl, dns_rdata_t **rdata)
{
	REQUIRE(name != NULL && *name == NULL);
	REQUIRE(it->result == ISC_R_SUCCESS);
	*name = dns_fixedname_name(&it->fixedname);
	*ttl = it->rdataset.ttl;
	dns_rdata_reset(&it->rdata);
	dns_rdataset_current(&it->rdataset, &it->rdata);
	*rdata = &it->rdata;
}

/**************************************************************************/

/*% Log an RR (for debugging) */

static void
log_rr(dns_name_t *name, dns_rdata_t *rdata, isc_uint32_t ttl) {
	isc_result_t result;
	isc_buffer_t buf;
	char mem[2000];
	dns_rdatalist_t rdl;
	dns_rdataset_t rds;
	dns_rdata_t rd = DNS_RDATA_INIT;

	rdl.type = rdata->type;
	rdl.rdclass = rdata->rdclass;
	rdl.ttl = ttl;
	if (rdata->type == dns_rdatatype_sig ||
	    rdata->type == dns_rdatatype_rrsig)
		rdl.covers = dns_rdata_covers(rdata);
	else
		rdl.covers = dns_rdatatype_none;
	ISC_LIST_INIT(rdl.rdata);
	ISC_LINK_INIT(&rdl, link);
	dns_rdataset_init(&rds);
	dns_rdata_init(&rd);
	dns_rdata_clone(rdata, &rd);
	ISC_LIST_APPEND(rdl.rdata, &rd, link);
	RUNTIME_CHECK(dns_rdatalist_tordataset(&rdl, &rds) == ISC_R_SUCCESS);

	isc_buffer_init(&buf, mem, sizeof(mem));
	result = dns_rdataset_totext(&rds, name,
				     ISC_FALSE, ISC_FALSE, &buf);

	/*
	 * We could use xfrout_log(), but that would produce
	 * very long lines with a repetitive prefix.
	 */
	if (result == ISC_R_SUCCESS) {
		/*
		 * Get rid of final newline.
		 */
		INSIST(buf.used >= 1 &&
		       ((char *) buf.base)[buf.used - 1] == '\n');
		buf.used--;

		isc_log_write(XFROUT_RR_LOGARGS, "%.*s",
			      (int)isc_buffer_usedlength(&buf),
			      (char *)isc_buffer_base(&buf));
	} else {
		isc_log_write(XFROUT_RR_LOGARGS, "<RR too large to print>");
	}
}

/**************************************************************************/
/*
 * An 'rrstream_t' is a polymorphic iterator that returns
 * a stream of resource records.  There are multiple implementations,
 * e.g. for generating AXFR and IXFR records streams.
 */

typedef struct rrstream_methods rrstream_methods_t;

typedef struct rrstream {
	isc_mem_t 		*mctx;
	rrstream_methods_t	*methods;
} rrstream_t;

struct rrstream_methods {
	isc_result_t 		(*first)(rrstream_t *);
	isc_result_t 		(*next)(rrstream_t *);
	void			(*current)(rrstream_t *,
					   dns_name_t **,
					   isc_uint32_t *,
					   dns_rdata_t **);
	void	 		(*pause)(rrstream_t *);
	void 			(*destroy)(rrstream_t **);
};

static void
rrstream_noop_pause(rrstream_t *rs) {
	UNUSED(rs);
}

/**************************************************************************/
/*
 * An 'ixfr_rrstream_t' is an 'rrstream_t' that returns
 * an IXFR-like RR stream from a journal file.
 *
 * The SOA at the beginning of each sequence of additions
 * or deletions are included in the stream, but the extra
 * SOAs at the beginning and end of the entire transfer are
 * not included.
 */

typedef struct ixfr_rrstream {
	rrstream_t		common;
	dns_journal_t 		*journal;
} ixfr_rrstream_t;

/* Forward declarations. */
static void
ixfr_rrstream_destroy(rrstream_t **sp);

static rrstream_methods_t ixfr_rrstream_methods;

/*
 * Returns: anything dns_journal_open() or dns_journal_iter_init()
 * may return.
 */

static isc_result_t
ixfr_rrstream_create(isc_mem_t *mctx,
		     const char *journal_filename,
		     isc_uint32_t begin_serial,
		     isc_uint32_t end_serial,
		     rrstream_t **sp)
{
	ixfr_rrstream_t *s;
	isc_result_t result;

	INSIST(sp != NULL && *sp == NULL);

	s = isc_mem_get(mctx, sizeof(*s));
	if (s == NULL)
		return (ISC_R_NOMEMORY);
	s->common.mctx = mctx;
	s->common.methods = &ixfr_rrstream_methods;
	s->journal = NULL;

	CHECK(dns_journal_open(mctx, journal_filename,
			       ISC_FALSE, &s->journal));
	CHECK(dns_journal_iter_init(s->journal, begin_serial, end_serial));

	*sp = (rrstream_t *) s;
	return (ISC_R_SUCCESS);

 failure:
	ixfr_rrstream_destroy((rrstream_t **) (void *)&s);
	return (result);
}

static isc_result_t
ixfr_rrstream_first(rrstream_t *rs) {
	ixfr_rrstream_t *s = (ixfr_rrstream_t *) rs;
	return (dns_journal_first_rr(s->journal));
}

static isc_result_t
ixfr_rrstream_next(rrstream_t *rs) {
	ixfr_rrstream_t *s = (ixfr_rrstream_t *) rs;
	return (dns_journal_next_rr(s->journal));
}

static void
ixfr_rrstream_current(rrstream_t *rs,
		       dns_name_t **name, isc_uint32_t *ttl,
		       dns_rdata_t **rdata)
{
	ixfr_rrstream_t *s = (ixfr_rrstream_t *) rs;
	dns_journal_current_rr(s->journal, name, ttl, rdata);
}

static void
ixfr_rrstream_destroy(rrstream_t **rsp) {
	ixfr_rrstream_t *s = (ixfr_rrstream_t *) *rsp;
	if (s->journal != 0)
		dns_journal_destroy(&s->journal);
	isc_mem_put(s->common.mctx, s, sizeof(*s));
}

static rrstream_methods_t ixfr_rrstream_methods = {
	ixfr_rrstream_first,
	ixfr_rrstream_next,
	ixfr_rrstream_current,
	rrstream_noop_pause,
	ixfr_rrstream_destroy
};

/**************************************************************************/
/*
 * An 'axfr_rrstream_t' is an 'rrstream_t' that returns
 * an AXFR-like RR stream from a database.
 *
 * The SOAs at the beginning and end of the transfer are
 * not included in the stream.
 */

typedef struct axfr_rrstream {
	rrstream_t		common;
	db_rr_iterator_t	it;
	isc_boolean_t		it_valid;
} axfr_rrstream_t;

/*
 * Forward declarations.
 */
static void
axfr_rrstream_destroy(rrstream_t **rsp);

static rrstream_methods_t axfr_rrstream_methods;

static isc_result_t
axfr_rrstream_create(isc_mem_t *mctx, dns_db_t *db, dns_dbversion_t *ver,
		     rrstream_t **sp)
{
	axfr_rrstream_t *s;
	isc_result_t result;

	INSIST(sp != NULL && *sp == NULL);

	s = isc_mem_get(mctx, sizeof(*s));
	if (s == NULL)
		return (ISC_R_NOMEMORY);
	s->common.mctx = mctx;
	s->common.methods = &axfr_rrstream_methods;
	s->it_valid = ISC_FALSE;

	CHECK(db_rr_iterator_init(&s->it, db, ver, 0));
	s->it_valid = ISC_TRUE;

	*sp = (rrstream_t *) s;
	return (ISC_R_SUCCESS);

 failure:
	axfr_rrstream_destroy((rrstream_t **) (void *)&s);
	return (result);
}

static isc_result_t
axfr_rrstream_first(rrstream_t *rs) {
	axfr_rrstream_t *s = (axfr_rrstream_t *) rs;
	isc_result_t result;
	result = db_rr_iterator_first(&s->it);
	if (result != ISC_R_SUCCESS)
		return (result);
	/* Skip SOA records. */
	for (;;) {
		dns_name_t *name_dummy = NULL;
		isc_uint32_t ttl_dummy;
		dns_rdata_t *rdata = NULL;
		db_rr_iterator_current(&s->it, &name_dummy,
				      &ttl_dummy, &rdata);
		if (rdata->type != dns_rdatatype_soa)
			break;
		result = db_rr_iterator_next(&s->it);
		if (result != ISC_R_SUCCESS)
			break;
	}
	return (result);
}

static isc_result_t
axfr_rrstream_next(rrstream_t *rs) {
	axfr_rrstream_t *s = (axfr_rrstream_t *) rs;
	isc_result_t result;

	/* Skip SOA records. */
	for (;;) {
		dns_name_t *name_dummy = NULL;
		isc_uint32_t ttl_dummy;
		dns_rdata_t *rdata = NULL;
		result = db_rr_iterator_next(&s->it);
		if (result != ISC_R_SUCCESS)
			break;
		db_rr_iterator_current(&s->it, &name_dummy,
				      &ttl_dummy, &rdata);
		if (rdata->type != dns_rdatatype_soa)
			break;
	}
	return (result);
}

static void
axfr_rrstream_current(rrstream_t *rs, dns_name_t **name, isc_uint32_t *ttl,
		      dns_rdata_t **rdata)
{
	axfr_rrstream_t *s = (axfr_rrstream_t *) rs;
	db_rr_iterator_current(&s->it, name, ttl, rdata);
}

static void
axfr_rrstream_pause(rrstream_t *rs) {
	axfr_rrstream_t *s = (axfr_rrstream_t *) rs;
	db_rr_iterator_pause(&s->it);
}

static void
axfr_rrstream_destroy(rrstream_t **rsp) {
	axfr_rrstream_t *s = (axfr_rrstream_t *) *rsp;
	if (s->it_valid)
		db_rr_iterator_destroy(&s->it);
	isc_mem_put(s->common.mctx, s, sizeof(*s));
}

static rrstream_methods_t axfr_rrstream_methods = {
	axfr_rrstream_first,
	axfr_rrstream_next,
	axfr_rrstream_current,
	axfr_rrstream_pause,
	axfr_rrstream_destroy
};

/**************************************************************************/
/*
 * An 'soa_rrstream_t' is a degenerate 'rrstream_t' that returns
 * a single SOA record.
 */

typedef struct soa_rrstream {
	rrstream_t		common;
	dns_difftuple_t 	*soa_tuple;
} soa_rrstream_t;

/*
 * Forward declarations.
 */
static void
soa_rrstream_destroy(rrstream_t **rsp);

static rrstream_methods_t soa_rrstream_methods;

static isc_result_t
soa_rrstream_create(isc_mem_t *mctx, dns_db_t *db, dns_dbversion_t *ver,
		    rrstream_t **sp)
{
	soa_rrstream_t *s;
	isc_result_t result;

	INSIST(sp != NULL && *sp == NULL);

	s = isc_mem_get(mctx, sizeof(*s));
	if (s == NULL)
		return (ISC_R_NOMEMORY);
	s->common.mctx = mctx;
	s->common.methods = &soa_rrstream_methods;
	s->soa_tuple = NULL;

	CHECK(dns_db_createsoatuple(db, ver, mctx, DNS_DIFFOP_EXISTS,
				    &s->soa_tuple));

	*sp = (rrstream_t *) s;
	return (ISC_R_SUCCESS);

 failure:
	soa_rrstream_destroy((rrstream_t **) (void *)&s);
	return (result);
}

static isc_result_t
soa_rrstream_first(rrstream_t *rs) {
	UNUSED(rs);
	return (ISC_R_SUCCESS);
}

static isc_result_t
soa_rrstream_next(rrstream_t *rs) {
	UNUSED(rs);
	return (ISC_R_NOMORE);
}

static void
soa_rrstream_current(rrstream_t *rs, dns_name_t **name, isc_uint32_t *ttl,
		     dns_rdata_t **rdata)
{
	soa_rrstream_t *s = (soa_rrstream_t *) rs;
	*name = &s->soa_tuple->name;
	*ttl = s->soa_tuple->ttl;
	*rdata = &s->soa_tuple->rdata;
}

static void
soa_rrstream_destroy(rrstream_t **rsp) {
	soa_rrstream_t *s = (soa_rrstream_t *) *rsp;
	if (s->soa_tuple != NULL)
		dns_difftuple_free(&s->soa_tuple);
	isc_mem_put(s->common.mctx, s, sizeof(*s));
}

static rrstream_methods_t soa_rrstream_methods = {
	soa_rrstream_first,
	soa_rrstream_next,
	soa_rrstream_current,
	rrstream_noop_pause,
	soa_rrstream_destroy
};

/**************************************************************************/
/*
 * A 'compound_rrstream_t' objects owns a soa_rrstream
 * and another rrstream, the "data stream".  It returns
 * a concatenated stream consisting of the soa_rrstream, then
 * the data stream, then the soa_rrstream again.
 *
 * The component streams are owned by the compound_rrstream_t
 * and are destroyed with it.
 */

typedef struct compound_rrstream {
	rrstream_t		common;
	rrstream_t		*components[3];
	int			state;
	isc_result_t		result;
} compound_rrstream_t;

/*
 * Forward declarations.
 */
static void
compound_rrstream_destroy(rrstream_t **rsp);

static isc_result_t
compound_rrstream_next(rrstream_t *rs);

static rrstream_methods_t compound_rrstream_methods;

/*
 * Requires:
 *	soa_stream != NULL && *soa_stream != NULL
 *	data_stream != NULL && *data_stream != NULL
 *	sp != NULL && *sp == NULL
 *
 * Ensures:
 *	*soa_stream == NULL
 *	*data_stream == NULL
 *	*sp points to a valid compound_rrstream_t
 *	The soa and data streams will be destroyed
 *	when the compound_rrstream_t is destroyed.
 */
static isc_result_t
compound_rrstream_create(isc_mem_t *mctx, rrstream_t **soa_stream,
			 rrstream_t **data_stream, rrstream_t **sp)
{
	compound_rrstream_t *s;

	INSIST(sp != NULL && *sp == NULL);

	s = isc_mem_get(mctx, sizeof(*s));
	if (s == NULL)
		return (ISC_R_NOMEMORY);
	s->common.mctx = mctx;
	s->common.methods = &compound_rrstream_methods;
	s->components[0] = *soa_stream;
	s->components[1] = *data_stream;
	s->components[2] = *soa_stream;
	s->state = -1;
	s->result = ISC_R_FAILURE;

	*soa_stream = NULL;
	*data_stream = NULL;
	*sp = (rrstream_t *) s;
	return (ISC_R_SUCCESS);
}

static isc_result_t
compound_rrstream_first(rrstream_t *rs) {
	compound_rrstream_t *s = (compound_rrstream_t *) rs;
	s->state = 0;
	do {
		rrstream_t *curstream = s->components[s->state];
		s->result = curstream->methods->first(curstream);
	} while (s->result == ISC_R_NOMORE && s->state < 2);
	return (s->result);
}

static isc_result_t
compound_rrstream_next(rrstream_t *rs) {
	compound_rrstream_t *s = (compound_rrstream_t *) rs;
	rrstream_t *curstream = s->components[s->state];
	s->result = curstream->methods->next(curstream);
	while (s->result == ISC_R_NOMORE) {
		/*
		 * Make sure locks held by the current stream
		 * are released before we switch streams.
		 */
		curstream->methods->pause(curstream);
		if (s->state == 2)
			return (ISC_R_NOMORE);
		s->state++;
		curstream = s->components[s->state];
		s->result = curstream->methods->first(curstream);
	}
	return (s->result);
}

static void
compound_rrstream_current(rrstream_t *rs, dns_name_t **name, isc_uint32_t *ttl,
			  dns_rdata_t **rdata)
{
	compound_rrstream_t *s = (compound_rrstream_t *) rs;
	rrstream_t *curstream;
	INSIST(0 <= s->state && s->state < 3);
	INSIST(s->result == ISC_R_SUCCESS);
	curstream = s->components[s->state];
	curstream->methods->current(curstream, name, ttl, rdata);
}

static void
compound_rrstream_pause(rrstream_t *rs)
{
	compound_rrstream_t *s = (compound_rrstream_t *) rs;
	rrstream_t *curstream;
	INSIST(0 <= s->state && s->state < 3);
	curstream = s->components[s->state];
	curstream->methods->pause(curstream);
}

static void
compound_rrstream_destroy(rrstream_t **rsp) {
	compound_rrstream_t *s = (compound_rrstream_t *) *rsp;
	s->components[0]->methods->destroy(&s->components[0]);
	s->components[1]->methods->destroy(&s->components[1]);
	s->components[2] = NULL; /* Copy of components[0]. */
	isc_mem_put(s->common.mctx, s, sizeof(*s));
}

static rrstream_methods_t compound_rrstream_methods = {
	compound_rrstream_first,
	compound_rrstream_next,
	compound_rrstream_current,
	compound_rrstream_pause,
	compound_rrstream_destroy
};

/**************************************************************************/
/*
 * An 'xfrout_ctx_t' contains the state of an outgoing AXFR or IXFR
 * in progress.
 */

typedef struct {
	isc_mem_t 		*mctx;
	ns_client_t		*client;
	unsigned int 		id;		/* ID of request */
	dns_name_t		*qname;		/* Question name of request */
	dns_rdatatype_t		qtype;		/* dns_rdatatype_{a,i}xfr */
	dns_rdataclass_t	qclass;
	dns_zone_t 		*zone;		/* (necessary for stats) */
	dns_db_t 		*db;
	dns_dbversion_t 	*ver;
	isc_quota_t		*quota;
	rrstream_t 		*stream;	/* The XFR RR stream */
	isc_boolean_t		end_of_stream;	/* EOS has been reached */
	isc_buffer_t 		buf;		/* Buffer for message owner
						   names and rdatas */
	isc_buffer_t 		txlenbuf;	/* Transmit length buffer */
	isc_buffer_t		txbuf;		/* Transmit message buffer */
	void 			*txmem;
	unsigned int 		txmemlen;
	unsigned int		nmsg;		/* Number of messages sent */
	dns_tsigkey_t		*tsigkey;	/* Key used to create TSIG */
	isc_buffer_t		*lasttsig;	/* the last TSIG */
	isc_boolean_t		many_answers;
	int			sends;		/* Send in progress */
	isc_boolean_t		shuttingdown;
	const char		*mnemonic;	/* Style of transfer */
} xfrout_ctx_t;

static isc_result_t
xfrout_ctx_create(isc_mem_t *mctx, ns_client_t *client,
		  unsigned int id, dns_name_t *qname, dns_rdatatype_t qtype,
		  dns_rdataclass_t qclass, dns_zone_t *zone,
		  dns_db_t *db, dns_dbversion_t *ver, isc_quota_t *quota,
		  rrstream_t *stream, dns_tsigkey_t *tsigkey,
		  isc_buffer_t *lasttsig,
		  unsigned int maxtime,
		  unsigned int idletime,
		  isc_boolean_t many_answers,
		  xfrout_ctx_t **xfrp);

static void
sendstream(xfrout_ctx_t *xfr);

static void
xfrout_senddone(isc_task_t *task, isc_event_t *event);

static void
xfrout_fail(xfrout_ctx_t *xfr, isc_result_t result, const char *msg);

static void
xfrout_maybe_destroy(xfrout_ctx_t *xfr);

static void
xfrout_ctx_destroy(xfrout_ctx_t **xfrp);

static void
xfrout_client_shutdown(void *arg, isc_result_t result);

static void
xfrout_log1(ns_client_t *client, dns_name_t *zonename,
	    dns_rdataclass_t rdclass, int level,
	    const char *fmt, ...) ISC_FORMAT_PRINTF(5, 6);

static void
xfrout_log(xfrout_ctx_t *xfr, int level, const char *fmt, ...)
	   ISC_FORMAT_PRINTF(3, 4);

/**************************************************************************/

void
ns_xfr_start(ns_client_t *client, dns_rdatatype_t reqtype) {
	isc_result_t result;
	dns_name_t *question_name;
	dns_rdataset_t *question_rdataset;
	dns_zone_t *zone = NULL;
	dns_db_t *db = NULL;
	dns_dbversion_t *ver = NULL;
	dns_rdataclass_t question_class;
	rrstream_t *soa_stream = NULL;
	rrstream_t *data_stream = NULL;
	rrstream_t *stream = NULL;
	dns_difftuple_t *current_soa_tuple = NULL;
	dns_name_t *soa_name;
	dns_rdataset_t *soa_rdataset;
	dns_rdata_t soa_rdata = DNS_RDATA_INIT;
	isc_boolean_t have_soa = ISC_FALSE;
	const char *mnemonic = NULL;
	isc_mem_t *mctx = client->mctx;
	dns_message_t *request = client->message;
	xfrout_ctx_t *xfr = NULL;
	isc_quota_t *quota = NULL;
	dns_transfer_format_t format = client->view->transfer_format;
	isc_netaddr_t na;
	dns_peer_t *peer = NULL;
	isc_buffer_t *tsigbuf = NULL;
	char *journalfile;
	char msg[NS_CLIENT_ACLMSGSIZE("zone transfer")];
	char keyname[DNS_NAME_FORMATSIZE];
	isc_boolean_t is_poll = ISC_FALSE;
#ifdef DLZ
	isc_boolean_t is_dlz = ISC_FALSE;
#endif

	switch (reqtype) {
	case dns_rdatatype_axfr:
		mnemonic = "AXFR";
		break;
	case dns_rdatatype_ixfr:
		mnemonic = "IXFR";
		break;
	default:
		INSIST(0);
		break;
	}

	ns_client_log(client,
		      DNS_LOGCATEGORY_XFER_OUT, NS_LOGMODULE_XFER_OUT,
		      ISC_LOG_DEBUG(6), "%s request", mnemonic);
	/*
	 * Apply quota.
	 */
	result = isc_quota_attach(&ns_g_server->xfroutquota, &quota);
	if (result != ISC_R_SUCCESS) {
		isc_log_write(XFROUT_COMMON_LOGARGS, ISC_LOG_WARNING,
			      "%s request denied: %s", mnemonic,
			      isc_result_totext(result));
		goto failure;
	}

	/*
	 * Interpret the question section.
	 */
	result = dns_message_firstname(request, DNS_SECTION_QUESTION);
	INSIST(result == ISC_R_SUCCESS);

	/*
	 * The question section must contain exactly one question, and
	 * it must be for AXFR/IXFR as appropriate.
	 */
	question_name = NULL;
	dns_message_currentname(request, DNS_SECTION_QUESTION, &question_name);
	question_rdataset = ISC_LIST_HEAD(question_name->list);
	question_class = question_rdataset->rdclass;
	INSIST(question_rdataset->type == reqtype);
	if (ISC_LIST_NEXT(question_rdataset, link) != NULL)
		FAILC(DNS_R_FORMERR, "multiple questions");
	result = dns_message_nextname(request, DNS_SECTION_QUESTION);
	if (result != ISC_R_NOMORE)
		FAILC(DNS_R_FORMERR, "multiple questions");

	result = dns_zt_find(client->view->zonetable, question_name, 0, NULL,
			     &zone);

	if (result != ISC_R_SUCCESS)
#ifdef DLZ
	{
		/*
		 * Normal zone table does not have a match.
		 * Try the DLZ database
		 */
		if (client->view->dlzdatabase != NULL) {
			result = dns_dlzallowzonexfr(client->view,
						     question_name,
						     &client->peeraddr,
						     &db);

			if (result == ISC_R_NOPERM) {
				char _buf1[DNS_NAME_FORMATSIZE];
				char _buf2[DNS_RDATACLASS_FORMATSIZE];

				result = DNS_R_REFUSED;
				dns_name_format(question_name, _buf1,
						sizeof(_buf1));
				dns_rdataclass_format(question_class,
						      _buf2, sizeof(_buf2));
				ns_client_log(client, DNS_LOGCATEGORY_SECURITY,
					      NS_LOGMODULE_XFER_OUT,
					      ISC_LOG_ERROR,
					      "zone transfer '%s/%s' denied",
					      _buf1, _buf2);
				goto failure;
			}
			if (result != ISC_R_SUCCESS)
#endif
			FAILQ(DNS_R_NOTAUTH, "non-authoritative zone",
				  question_name, question_class);
#ifdef DLZ
			is_dlz = ISC_TRUE;
			/*
			 * DLZ only support full zone transfer, not incremental
			 */
			if (reqtype != dns_rdatatype_axfr) {
				mnemonic = "AXFR-style IXFR";
				reqtype = dns_rdatatype_axfr;
			}

		} else {
			/*
			 * not DLZ and not in normal zone table, we are
			 * not authoritative
			 */
			FAILQ(DNS_R_NOTAUTH, "non-authoritative zone",
			      question_name, question_class);
		}
	} else {
		/* zone table has a match */
#endif
		switch(dns_zone_gettype(zone)) {
			case dns_zone_master:
			case dns_zone_slave:
				break;	/* Master and slave zones are OK for transfer. */
			default:
				FAILQ(DNS_R_NOTAUTH, "non-authoritative zone", question_name, question_class);
			}
		CHECK(dns_zone_getdb(zone, &db));
		dns_db_currentversion(db, &ver);
#ifdef DLZ
	}
#endif

	xfrout_log1(client, question_name, question_class, ISC_LOG_DEBUG(6),
		    "%s question section OK", mnemonic);

	/*
	 * Check the authority section.  Look for a SOA record with
	 * the same name and class as the question.
	 */
	for (result = dns_message_firstname(request, DNS_SECTION_AUTHORITY);
	     result == ISC_R_SUCCESS;
	     result = dns_message_nextname(request, DNS_SECTION_AUTHORITY))
	{
		soa_name = NULL;
		dns_message_currentname(request, DNS_SECTION_AUTHORITY,
					&soa_name);

		/*
		 * Ignore data whose owner name is not the zone apex.
		 */
		if (! dns_name_equal(soa_name, question_name))
			continue;

		for (soa_rdataset = ISC_LIST_HEAD(soa_name->list);
		     soa_rdataset != NULL;
		     soa_rdataset = ISC_LIST_NEXT(soa_rdataset, link))
		{
			/*
			 * Ignore non-SOA data.
			 */
			if (soa_rdataset->type != dns_rdatatype_soa)
				continue;
			if (soa_rdataset->rdclass != question_class)
				continue;

			CHECK(dns_rdataset_first(soa_rdataset));
			dns_rdataset_current(soa_rdataset, &soa_rdata);
			result = dns_rdataset_next(soa_rdataset);
			if (result == ISC_R_SUCCESS)
				FAILC(DNS_R_FORMERR,
				      "IXFR authority section "
				      "has multiple SOAs");
			have_soa = ISC_TRUE;
			goto got_soa;
		}
	}
 got_soa:
	if (result != ISC_R_NOMORE)
		CHECK(result);

	xfrout_log1(client, question_name, question_class, ISC_LOG_DEBUG(6),
		    "%s authority section OK", mnemonic);

	/*
	 * Decide whether to allow this transfer.
	 */
#ifdef DLZ
	/*
	 * if not a DLZ zone decide whether to allow this transfer.
	 */
	if (!is_dlz) {
#endif
		ns_client_aclmsg("zone transfer", question_name, reqtype,
				 client->view->rdclass, msg, sizeof(msg));
		CHECK(ns_client_checkacl(client, NULL, msg,
					 dns_zone_getxfracl(zone),
					 ISC_TRUE, ISC_LOG_ERROR));
#ifdef DLZ
	}
#endif

	/*
	 * AXFR over UDP is not possible.
	 */
	if (reqtype == dns_rdatatype_axfr &&
	    (client->attributes & NS_CLIENTATTR_TCP) == 0)
		FAILC(DNS_R_FORMERR, "attempted AXFR over UDP");

	/*
	 * Look up the requesting server in the peer table.
	 */
	isc_netaddr_fromsockaddr(&na, &client->peeraddr);
	(void)dns_peerlist_peerbyaddr(client->view->peers, &na, &peer);

	/*
	 * Decide on the transfer format (one-answer or many-answers).
	 */
	if (peer != NULL)
		(void)dns_peer_gettransferformat(peer, &format);

	/*
	 * Get a dynamically allocated copy of the current SOA.
	 */
#ifdef DLZ
	if (is_dlz)
		dns_db_currentversion(db, &ver);
#endif
	CHECK(dns_db_createsoatuple(db, ver, mctx, DNS_DIFFOP_EXISTS,
				    &current_soa_tuple));

	if (reqtype == dns_rdatatype_ixfr) {
		isc_uint32_t begin_serial, current_serial;
		isc_boolean_t provide_ixfr;

		/*
		 * Outgoing IXFR may have been disabled for this peer
		 * or globally.
		 */
		provide_ixfr = client->view->provideixfr;
		if (peer != NULL)
			(void) dns_peer_getprovideixfr(peer, &provide_ixfr);
		if (provide_ixfr == ISC_FALSE)
			goto axfr_fallback;

		if (! have_soa)
			FAILC(DNS_R_FORMERR,
			      "IXFR request missing SOA");

		begin_serial = dns_soa_getserial(&soa_rdata);
		current_serial = dns_soa_getserial(&current_soa_tuple->rdata);

		/*
		 * RFC1995 says "If an IXFR query with the same or
		 * newer version number than that of the server
		 * is received, it is replied to with a single SOA
		 * record of the server's current version, just as
		 * in AXFR".  The claim about AXFR is incorrect,
		 * but other than that, we do as the RFC says.
		 *
		 * Sending a single SOA record is also how we refuse
		 * IXFR over UDP (currently, we always do).
		 */
		if (DNS_SERIAL_GE(begin_serial, current_serial) ||
		    (client->attributes & NS_CLIENTATTR_TCP) == 0)
		{
			CHECK(soa_rrstream_create(mctx, db, ver, &stream));
			is_poll = ISC_TRUE;
			goto have_stream;
		}
		journalfile = dns_zone_getjournal(zone);
		if (journalfile != NULL)
			result = ixfr_rrstream_create(mctx,
						      journalfile,
						      begin_serial,
						      current_serial,
						      &data_stream);
		else
			result = ISC_R_NOTFOUND;
		if (result == ISC_R_NOTFOUND ||
		    result == ISC_R_RANGE) {
			xfrout_log1(client, question_name, question_class,
				    ISC_LOG_DEBUG(4),
				    "IXFR version not in journal, "
				    "falling back to AXFR");
			mnemonic = "AXFR-style IXFR";
			goto axfr_fallback;
		}
		CHECK(result);
	} else {
	axfr_fallback:
		CHECK(axfr_rrstream_create(mctx, db, ver,
					   &data_stream));
	}

	/*
	 * Bracket the data stream with SOAs.
	 */
	CHECK(soa_rrstream_create(mctx, db, ver, &soa_stream));
	CHECK(compound_rrstream_create(mctx, &soa_stream, &data_stream,
				       &stream));
	soa_stream = NULL;
	data_stream = NULL;

 have_stream:
	CHECK(dns_message_getquerytsig(request, mctx, &tsigbuf));
	/*
	 * Create the xfrout context object.  This transfers the ownership
	 * of "stream", "db", "ver", and "quota" to the xfrout context object.
	 */



#ifdef DLZ
	if (is_dlz)
		CHECK(xfrout_ctx_create(mctx, client, request->id,
					question_name, reqtype, question_class,
					zone, db, ver, quota, stream,
					dns_message_gettsigkey(request),
					tsigbuf,
					3600,
					3600,
					(format == dns_many_answers) ?
					ISC_TRUE : ISC_FALSE,
					&xfr));
	else
#endif
		CHECK(xfrout_ctx_create(mctx, client, request->id,
					question_name, reqtype, question_class,
					zone, db, ver, quota, stream,
					dns_message_gettsigkey(request),
					tsigbuf,
					dns_zone_getmaxxfrout(zone),
					dns_zone_getidleout(zone),
					(format == dns_many_answers) ?
					ISC_TRUE : ISC_FALSE,
					&xfr));

	xfr->mnemonic = mnemonic;
	stream = NULL;
	quota = NULL;

	CHECK(xfr->stream->methods->first(xfr->stream));

	if (xfr->tsigkey != NULL) {
		dns_name_format(&xfr->tsigkey->name, keyname, sizeof(keyname));
	} else
		keyname[0] = '\0';
	if (is_poll)
		xfrout_log1(client, question_name, question_class,
			    ISC_LOG_DEBUG(1), "IXFR poll up to date%s%s",
			    (xfr->tsigkey != NULL) ? ": TSIG " : "", keyname);
	else
		xfrout_log1(client, question_name, question_class,
			    ISC_LOG_INFO, "%s started%s%s", mnemonic,
			    (xfr->tsigkey != NULL) ? ": TSIG " : "", keyname);

	/*
	 * Hand the context over to sendstream().  Set xfr to NULL;
	 * sendstream() is responsible for either passing the
	 * context on to a later event handler or destroying it.
	 */
	sendstream(xfr);
	xfr = NULL;

	result = ISC_R_SUCCESS;

 failure:
	if (result == DNS_R_REFUSED)
		inc_stats(zone, dns_nsstatscounter_xfrrej);
	if (quota != NULL)
		isc_quota_detach(&quota);
	if (current_soa_tuple != NULL)
		dns_difftuple_free(&current_soa_tuple);
	if (stream != NULL)
		stream->methods->destroy(&stream);
	if (soa_stream != NULL)
		soa_stream->methods->destroy(&soa_stream);
	if (data_stream != NULL)
		data_stream->methods->destroy(&data_stream);
	if (ver != NULL)
		dns_db_closeversion(db, &ver, ISC_FALSE);
	if (db != NULL)
		dns_db_detach(&db);
	if (zone != NULL)
		dns_zone_detach(&zone);
	/* XXX kludge */
	if (xfr != NULL) {
		xfrout_fail(xfr, result, "setting up zone transfer");
	} else if (result != ISC_R_SUCCESS) {
		ns_client_log(client, DNS_LOGCATEGORY_XFER_OUT,
			      NS_LOGMODULE_XFER_OUT,
			      ISC_LOG_DEBUG(3), "zone transfer setup failed");
		ns_client_error(client, result);
	}
}

static isc_result_t
xfrout_ctx_create(isc_mem_t *mctx, ns_client_t *client, unsigned int id,
		  dns_name_t *qname, dns_rdatatype_t qtype,
		  dns_rdataclass_t qclass, dns_zone_t *zone,
		  dns_db_t *db, dns_dbversion_t *ver, isc_quota_t *quota,
		  rrstream_t *stream, dns_tsigkey_t *tsigkey,
		  isc_buffer_t *lasttsig, unsigned int maxtime,
		  unsigned int idletime, isc_boolean_t many_answers,
		  xfrout_ctx_t **xfrp)
{
	xfrout_ctx_t *xfr;
	isc_result_t result;
	unsigned int len;
	void *mem;

	INSIST(xfrp != NULL && *xfrp == NULL);
	xfr = isc_mem_get(mctx, sizeof(*xfr));
	if (xfr == NULL)
		return (ISC_R_NOMEMORY);
	xfr->mctx = mctx;
	xfr->client = NULL;
	ns_client_attach(client, &xfr->client);
	xfr->id = id;
	xfr->qname = qname;
	xfr->qtype = qtype;
	xfr->qclass = qclass;
	xfr->zone = NULL;
	xfr->db = NULL;
	xfr->ver = NULL;
	if (zone != NULL)	/* zone will be NULL if it's DLZ */
		dns_zone_attach(zone, &xfr->zone);
	dns_db_attach(db, &xfr->db);
	dns_db_attachversion(db, ver, &xfr->ver);
	xfr->end_of_stream = ISC_FALSE;
	xfr->tsigkey = tsigkey;
	xfr->lasttsig = lasttsig;
	xfr->txmem = NULL;
	xfr->txmemlen = 0;
	xfr->nmsg = 0;
	xfr->many_answers = many_answers,
	xfr->sends = 0;
	xfr->shuttingdown = ISC_FALSE;
	xfr->mnemonic = NULL;
	xfr->buf.base = NULL;
	xfr->buf.length = 0;
	xfr->txmem = NULL;
	xfr->txmemlen = 0;
	xfr->stream = NULL;
	xfr->quota = NULL;

	/*
	 * Allocate a temporary buffer for the uncompressed response
	 * message data.  The size should be no more than 65535 bytes
	 * so that the compressed data will fit in a TCP message,
	 * and no less than 65535 bytes so that an almost maximum-sized
	 * RR will fit.  Note that although 65535-byte RRs are allowed
	 * in principle, they cannot be zone-transferred (at least not
	 * if uncompressible), because the message and RR headers would
	 * push the size of the TCP message over the 65536 byte limit.
	 */
	len = 65535;
	mem = isc_mem_get(mctx, len);
	if (mem == NULL) {
		result = ISC_R_NOMEMORY;
		goto failure;
	}
	isc_buffer_init(&xfr->buf, mem, len);

	/*
	 * Allocate another temporary buffer for the compressed
	 * response message and its TCP length prefix.
	 */
	len = 2 + 65535;
	mem = isc_mem_get(mctx, len);
	if (mem == NULL) {
		result = ISC_R_NOMEMORY;
		goto failure;
	}
	isc_buffer_init(&xfr->txlenbuf, mem, 2);
	isc_buffer_init(&xfr->txbuf, (char *) mem + 2, len - 2);
	xfr->txmem = mem;
	xfr->txmemlen = len;

	CHECK(dns_timer_setidle(xfr->client->timer,
				maxtime, idletime, ISC_FALSE));

	/*
	 * Register a shutdown callback with the client, so that we
	 * can stop the transfer immediately when the client task
	 * gets a shutdown event.
	 */
	xfr->client->shutdown = xfrout_client_shutdown;
	xfr->client->shutdown_arg = xfr;
	/*
	 * These MUST be after the last "goto failure;" / CHECK to
	 * prevent a double free by the caller.
	 */
	xfr->quota = quota;
	xfr->stream = stream;

	*xfrp = xfr;
	return (ISC_R_SUCCESS);

failure:
	xfrout_ctx_destroy(&xfr);
	return (result);
}


/*
 * Arrange to send as much as we can of "stream" without blocking.
 *
 * Requires:
 *	The stream iterator is initialized and points at an RR,
 *      or possibly at the end of the stream (that is, the
 *      _first method of the iterator has been called).
 */
static void
sendstream(xfrout_ctx_t *xfr) {
	dns_message_t *tcpmsg = NULL;
	dns_message_t *msg = NULL; /* Client message if UDP, tcpmsg if TCP */
	isc_result_t result;
	isc_region_t used;
	isc_region_t region;
	dns_rdataset_t *qrdataset;
	dns_name_t *msgname = NULL;
	dns_rdata_t *msgrdata = NULL;
	dns_rdatalist_t *msgrdl = NULL;
	dns_rdataset_t *msgrds = NULL;
	dns_compress_t cctx;
	isc_boolean_t cleanup_cctx = ISC_FALSE;

	int n_rrs;

	isc_buffer_clear(&xfr->buf);
	isc_buffer_clear(&xfr->txlenbuf);
	isc_buffer_clear(&xfr->txbuf);

	if ((xfr->client->attributes & NS_CLIENTATTR_TCP) == 0) {
		/*
		 * In the UDP case, we put the response data directly into
		 * the client message.
		 */
		msg = xfr->client->message;
		CHECK(dns_message_reply(msg, ISC_TRUE));
	} else {
		/*
		 * TCP. Build a response dns_message_t, temporarily storing
		 * the raw, uncompressed owner names and RR data contiguously
		 * in xfr->buf.  We know that if the uncompressed data fits
		 * in xfr->buf, the compressed data will surely fit in a TCP
		 * message.
		 */

		CHECK(dns_message_create(xfr->mctx,
					 DNS_MESSAGE_INTENTRENDER, &tcpmsg));
		msg = tcpmsg;

		msg->id = xfr->id;
		msg->rcode = dns_rcode_noerror;
		msg->flags = DNS_MESSAGEFLAG_QR | DNS_MESSAGEFLAG_AA;
		if ((xfr->client->attributes & NS_CLIENTATTR_RA) != 0)
			msg->flags |= DNS_MESSAGEFLAG_RA;
		CHECK(dns_message_settsigkey(msg, xfr->tsigkey));
		CHECK(dns_message_setquerytsig(msg, xfr->lasttsig));
		if (xfr->lasttsig != NULL)
			isc_buffer_free(&xfr->lasttsig);

		/*
		 * Include a question section in the first message only.
		 * BIND 8.2.1 will not recognize an IXFR if it does not
		 * have a question section.
		 */
		if (xfr->nmsg == 0) {
			dns_name_t *qname = NULL;
			isc_region_t r;

			/*
			 * Reserve space for the 12-byte message header
			 * and 4 bytes of question.
			 */
			isc_buffer_add(&xfr->buf, 12 + 4);

			qrdataset = NULL;
			result = dns_message_gettemprdataset(msg, &qrdataset);
			if (result != ISC_R_SUCCESS)
				goto failure;
			dns_rdataset_init(qrdataset);
			dns_rdataset_makequestion(qrdataset,
					xfr->client->message->rdclass,
					xfr->qtype);

			result = dns_message_gettempname(msg, &qname);
			if (result != ISC_R_SUCCESS)
				goto failure;
			dns_name_init(qname, NULL);
			isc_buffer_availableregion(&xfr->buf, &r);
			INSIST(r.length >= xfr->qname->length);
			r.length = xfr->qname->length;
			isc_buffer_putmem(&xfr->buf, xfr->qname->ndata,
					  xfr->qname->length);
			dns_name_fromregion(qname, &r);
			ISC_LIST_INIT(qname->list);
			ISC_LIST_APPEND(qname->list, qrdataset, link);

			dns_message_addname(msg, qname, DNS_SECTION_QUESTION);
		}
		else
			msg->tcp_continuation = 1;
	}

	/*
	 * Try to fit in as many RRs as possible, unless "one-answer"
	 * format has been requested.
	 */
	for (n_rrs = 0; ; n_rrs++) {
		dns_name_t *name = NULL;
		isc_uint32_t ttl;
		dns_rdata_t *rdata = NULL;

		unsigned int size;
		isc_region_t r;

		msgname = NULL;
		msgrdata = NULL;
		msgrdl = NULL;
		msgrds = NULL;

		xfr->stream->methods->current(xfr->stream,
					      &name, &ttl, &rdata);
		size = name->length + 10 + rdata->length;
		isc_buffer_availableregion(&xfr->buf, &r);
		if (size >= r.length) {
			/*
			 * RR would not fit.  If there are other RRs in the
			 * buffer, send them now and leave this RR to the
			 * next message.  If this RR overflows the buffer
			 * all by itself, fail.
			 *
			 * In theory some RRs might fit in a TCP message
			 * when compressed even if they do not fit when
			 * uncompressed, but surely we don't want
			 * to send such monstrosities to an unsuspecting
			 * slave.
			 */
			if (n_rrs == 0) {
				xfrout_log(xfr, ISC_LOG_WARNING,
					   "RR too large for zone transfer "
					   "(%d bytes)", size);
				/* XXX DNS_R_RRTOOLARGE? */
				result = ISC_R_NOSPACE;
				goto failure;
			}
			break;
		}

		if (isc_log_wouldlog(ns_g_lctx, XFROUT_RR_LOGLEVEL))
			log_rr(name, rdata, ttl); /* XXX */

		result = dns_message_gettempname(msg, &msgname);
		if (result != ISC_R_SUCCESS)
			goto failure;
		dns_name_init(msgname, NULL);
		isc_buffer_availableregion(&xfr->buf, &r);
		INSIST(r.length >= name->length);
		r.length = name->length;
		isc_buffer_putmem(&xfr->buf, name->ndata, name->length);
		dns_name_fromregion(msgname, &r);

		/* Reserve space for RR header. */
		isc_buffer_add(&xfr->buf, 10);

		result = dns_message_gettemprdata(msg, &msgrdata);
		if (result != ISC_R_SUCCESS)
			goto failure;
		isc_buffer_availableregion(&xfr->buf, &r);
		r.length = rdata->length;
		isc_buffer_putmem(&xfr->buf, rdata->data, rdata->length);
		dns_rdata_init(msgrdata);
		dns_rdata_fromregion(msgrdata,
				     rdata->rdclass, rdata->type, &r);

		result = dns_message_gettemprdatalist(msg, &msgrdl);
		if (result != ISC_R_SUCCESS)
			goto failure;
		msgrdl->type = rdata->type;
		msgrdl->rdclass = rdata->rdclass;
		msgrdl->ttl = ttl;
		if (rdata->type == dns_rdatatype_sig ||
		    rdata->type == dns_rdatatype_rrsig)
			msgrdl->covers = dns_rdata_covers(rdata);
		else
			msgrdl->covers = dns_rdatatype_none;
		ISC_LINK_INIT(msgrdl, link);
		ISC_LIST_INIT(msgrdl->rdata);
		ISC_LIST_APPEND(msgrdl->rdata, msgrdata, link);

		result = dns_message_gettemprdataset(msg, &msgrds);
		if (result != ISC_R_SUCCESS)
			goto failure;
		dns_rdataset_init(msgrds);
		result = dns_rdatalist_tordataset(msgrdl, msgrds);
		INSIST(result == ISC_R_SUCCESS);

		ISC_LIST_APPEND(msgname->list, msgrds, link);

		dns_message_addname(msg, msgname, DNS_SECTION_ANSWER);
		msgname = NULL;

		result = xfr->stream->methods->next(xfr->stream);
		if (result == ISC_R_NOMORE) {
			xfr->end_of_stream = ISC_TRUE;
			break;
		}
		CHECK(result);

		if (! xfr->many_answers)
			break;
	}

	if ((xfr->client->attributes & NS_CLIENTATTR_TCP) != 0) {
		CHECK(dns_compress_init(&cctx, -1, xfr->mctx));
		dns_compress_setsensitive(&cctx, ISC_TRUE);
		cleanup_cctx = ISC_TRUE;
		CHECK(dns_message_renderbegin(msg, &cctx, &xfr->txbuf));
		CHECK(dns_message_rendersection(msg, DNS_SECTION_QUESTION, 0));
		CHECK(dns_message_rendersection(msg, DNS_SECTION_ANSWER, 0));
		CHECK(dns_message_renderend(msg));
		dns_compress_invalidate(&cctx);
		cleanup_cctx = ISC_FALSE;

		isc_buffer_usedregion(&xfr->txbuf, &used);
		isc_buffer_putuint16(&xfr->txlenbuf,
				     (isc_uint16_t)used.length);
		region.base = xfr->txlenbuf.base;
		region.length = 2 + used.length;
		xfrout_log(xfr, ISC_LOG_DEBUG(8),
			   "sending TCP message of %d bytes",
			   used.length);
		CHECK(isc_socket_send(xfr->client->tcpsocket, /* XXX */
				      &region, xfr->client->task,
				      xfrout_senddone,
				      xfr));
		xfr->sends++;
	} else {
		xfrout_log(xfr, ISC_LOG_DEBUG(8), "sending IXFR UDP response");
		ns_client_send(xfr->client);
		xfr->stream->methods->pause(xfr->stream);
		xfrout_ctx_destroy(&xfr);
		return;
	}

	/* Advance lasttsig to be the last TSIG generated */
	CHECK(dns_message_getquerytsig(msg, xfr->mctx, &xfr->lasttsig));

	xfr->nmsg++;

 failure:
	if (msgname != NULL) {
		if (msgrds != NULL) {
			if (dns_rdataset_isassociated(msgrds))
				dns_rdataset_disassociate(msgrds);
			dns_message_puttemprdataset(msg, &msgrds);
		}
		if (msgrdl != NULL) {
			ISC_LIST_UNLINK(msgrdl->rdata, msgrdata, link);
			dns_message_puttemprdatalist(msg, &msgrdl);
		}
		if (msgrdata != NULL)
			dns_message_puttemprdata(msg, &msgrdata);
		dns_message_puttempname(msg, &msgname);
	}

	if (tcpmsg != NULL)
		dns_message_destroy(&tcpmsg);

	if (cleanup_cctx)
		dns_compress_invalidate(&cctx);
	/*
	 * Make sure to release any locks held by database
	 * iterators before returning from the event handler.
	 */
	xfr->stream->methods->pause(xfr->stream);

	if (result == ISC_R_SUCCESS)
		return;

	xfrout_fail(xfr, result, "sending zone data");
}

static void
xfrout_ctx_destroy(xfrout_ctx_t **xfrp) {
	xfrout_ctx_t *xfr = *xfrp;

	INSIST(xfr->sends == 0);

	xfr->client->shutdown = NULL;
	xfr->client->shutdown_arg = NULL;

	if (xfr->stream != NULL)
		xfr->stream->methods->destroy(&xfr->stream);
	if (xfr->buf.base != NULL)
		isc_mem_put(xfr->mctx, xfr->buf.base, xfr->buf.length);
	if (xfr->txmem != NULL)
		isc_mem_put(xfr->mctx, xfr->txmem, xfr->txmemlen);
	if (xfr->lasttsig != NULL)
		isc_buffer_free(&xfr->lasttsig);
	if (xfr->quota != NULL)
		isc_quota_detach(&xfr->quota);
	if (xfr->ver != NULL)
		dns_db_closeversion(xfr->db, &xfr->ver, ISC_FALSE);
	if (xfr->zone != NULL)
		dns_zone_detach(&xfr->zone);
	if (xfr->db != NULL)
		dns_db_detach(&xfr->db);

	ns_client_detach(&xfr->client);

	isc_mem_put(xfr->mctx, xfr, sizeof(*xfr));

	*xfrp = NULL;
}

static void
xfrout_senddone(isc_task_t *task, isc_event_t *event) {
	isc_socketevent_t *sev = (isc_socketevent_t *)event;
	xfrout_ctx_t *xfr = (xfrout_ctx_t *)event->ev_arg;
	isc_result_t evresult = sev->result;

	UNUSED(task);

	INSIST(event->ev_type == ISC_SOCKEVENT_SENDDONE);

	isc_event_free(&event);
	xfr->sends--;
	INSIST(xfr->sends == 0);

	(void)isc_timer_touch(xfr->client->timer);
	if (xfr->shuttingdown == ISC_TRUE) {
		xfrout_maybe_destroy(xfr);
	} else if (evresult != ISC_R_SUCCESS) {
		xfrout_fail(xfr, evresult, "send");
	} else if (xfr->end_of_stream == ISC_FALSE) {
		sendstream(xfr);
	} else {
		/* End of zone transfer stream. */
		inc_stats(xfr->zone, dns_nsstatscounter_xfrdone);
		xfrout_log(xfr, ISC_LOG_INFO, "%s ended", xfr->mnemonic);
		ns_client_next(xfr->client, ISC_R_SUCCESS);
		xfrout_ctx_destroy(&xfr);
	}
}

static void
xfrout_fail(xfrout_ctx_t *xfr, isc_result_t result, const char *msg) {
	xfr->shuttingdown = ISC_TRUE;
	xfrout_log(xfr, ISC_LOG_ERROR, "%s: %s",
		   msg, isc_result_totext(result));
	xfrout_maybe_destroy(xfr);
}

static void
xfrout_maybe_destroy(xfrout_ctx_t *xfr) {
	INSIST(xfr->shuttingdown == ISC_TRUE);
	if (xfr->sends > 0) {
		/*
		 * If we are currently sending, cancel it and wait for
		 * cancel event before destroying the context.
		 */
		isc_socket_cancel(xfr->client->tcpsocket, xfr->client->task,
				  ISC_SOCKCANCEL_SEND);
	} else {
		ns_client_next(xfr->client, ISC_R_CANCELED);
		xfrout_ctx_destroy(&xfr);
	}
}

static void
xfrout_client_shutdown(void *arg, isc_result_t result) {
	xfrout_ctx_t *xfr = (xfrout_ctx_t *) arg;
	xfrout_fail(xfr, result, "aborted");
}

/*
 * Log outgoing zone transfer messages in a format like
 * <client>: transfer of <zone>: <message>
 */

static void
xfrout_logv(ns_client_t *client, dns_name_t *zonename,
	    dns_rdataclass_t rdclass, int level, const char *fmt, va_list ap)
     ISC_FORMAT_PRINTF(5, 0);

static void
xfrout_logv(ns_client_t *client, dns_name_t *zonename,
	    dns_rdataclass_t rdclass, int level, const char *fmt, va_list ap)
{
	char msgbuf[2048];
	char namebuf[DNS_NAME_FORMATSIZE];
	char classbuf[DNS_RDATACLASS_FORMATSIZE];

	dns_name_format(zonename, namebuf, sizeof(namebuf));
	dns_rdataclass_format(rdclass, classbuf, sizeof(classbuf));
	vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
	ns_client_log(client, DNS_LOGCATEGORY_XFER_OUT,
		      NS_LOGMODULE_XFER_OUT, level,
		      "transfer of '%s/%s': %s", namebuf, classbuf, msgbuf);
}

/*
 * Logging function for use when a xfrout_ctx_t has not yet been created.
 */
static void
xfrout_log1(ns_client_t *client, dns_name_t *zonename,
	    dns_rdataclass_t rdclass, int level, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	xfrout_logv(client, zonename, rdclass, level, fmt, ap);
	va_end(ap);
}

/*
 * Logging function for use when there is a xfrout_ctx_t.
 */
static void
xfrout_log(xfrout_ctx_t *xfr, int level, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	xfrout_logv(xfr->client, xfr->qname, xfr->qclass, level, fmt, ap);
	va_end(ap);
}
