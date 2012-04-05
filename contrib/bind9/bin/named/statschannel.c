/*
 * Copyright (C) 2008-2011  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: statschannel.c,v 1.26.150.2 2011/03/12 04:59:14 tbox Exp $ */

/*! \file */

#include <config.h>

#include <isc/buffer.h>
#include <isc/httpd.h>
#include <isc/mem.h>
#include <isc/once.h>
#include <isc/print.h>
#include <isc/socket.h>
#include <isc/stats.h>
#include <isc/task.h>

#include <dns/cache.h>
#include <dns/db.h>
#include <dns/opcode.h>
#include <dns/resolver.h>
#include <dns/rdataclass.h>
#include <dns/rdatatype.h>
#include <dns/stats.h>
#include <dns/view.h>
#include <dns/zt.h>

#include <named/log.h>
#include <named/server.h>
#include <named/statschannel.h>

#include "bind9.xsl.h"

struct ns_statschannel {
	/* Unlocked */
	isc_httpdmgr_t				*httpdmgr;
	isc_sockaddr_t				address;
	isc_mem_t				*mctx;

	/*
	 * Locked by channel lock: can be referenced and modified by both
	 * the server task and the channel task.
	 */
	isc_mutex_t				lock;
	dns_acl_t				*acl;

	/* Locked by server task */
	ISC_LINK(struct ns_statschannel)	link;
};

typedef enum { statsformat_file, statsformat_xml } statsformat_t;

typedef struct
stats_dumparg {
	statsformat_t	type;
	void		*arg;		/* type dependent argument */
	int		ncounters;	/* used for general statistics */
	int		*counterindices; /* used for general statistics */
	isc_uint64_t	*countervalues;	 /* used for general statistics */
	isc_result_t	result;
} stats_dumparg_t;

static isc_once_t once = ISC_ONCE_INIT;

/*%
 * Statistics descriptions.  These could be statistically initialized at
 * compile time, but we configure them run time in the init_desc() function
 * below so that they'll be less susceptible to counter name changes.
 */
static const char *nsstats_desc[dns_nsstatscounter_max];
static const char *resstats_desc[dns_resstatscounter_max];
static const char *zonestats_desc[dns_zonestatscounter_max];
static const char *sockstats_desc[isc_sockstatscounter_max];
#ifdef HAVE_LIBXML2
static const char *nsstats_xmldesc[dns_nsstatscounter_max];
static const char *resstats_xmldesc[dns_resstatscounter_max];
static const char *zonestats_xmldesc[dns_zonestatscounter_max];
static const char *sockstats_xmldesc[isc_sockstatscounter_max];
#else
#define nsstats_xmldesc NULL
#define resstats_xmldesc NULL
#define zonestats_xmldesc NULL
#define sockstats_xmldesc NULL
#endif	/* HAVE_LIBXML2 */

#define TRY0(a) do { xmlrc = (a); if (xmlrc < 0) goto error; } while(0)

/*%
 * Mapping arrays to represent statistics counters in the order of our
 * preference, regardless of the order of counter indices.  For example,
 * nsstats_desc[nsstats_index[0]] will be the description that is shown first.
 */
static int nsstats_index[dns_nsstatscounter_max];
static int resstats_index[dns_resstatscounter_max];
static int zonestats_index[dns_zonestatscounter_max];
static int sockstats_index[isc_sockstatscounter_max];

static inline void
set_desc(int counter, int maxcounter, const char *fdesc, const char **fdescs,
	 const char *xdesc, const char **xdescs)
{
	REQUIRE(counter < maxcounter);
	REQUIRE(fdescs[counter] == NULL);
#ifdef HAVE_LIBXML2
	REQUIRE(xdescs[counter] == NULL);
#endif

	fdescs[counter] = fdesc;
#ifdef HAVE_LIBXML2
	xdescs[counter] = xdesc;
#else
	UNUSED(xdesc);
	UNUSED(xdescs);
#endif
}

static void
init_desc(void) {
	int i;

	/* Initialize name server statistics */
	for (i = 0; i < dns_nsstatscounter_max; i++)
		nsstats_desc[i] = NULL;
#ifdef HAVE_LIBXML2
	for (i = 0; i < dns_nsstatscounter_max; i++)
		nsstats_xmldesc[i] = NULL;
#endif

#define SET_NSSTATDESC(counterid, desc, xmldesc) \
	do { \
		set_desc(dns_nsstatscounter_ ## counterid, \
			 dns_nsstatscounter_max, \
			 desc, nsstats_desc, xmldesc, nsstats_xmldesc); \
		nsstats_index[i++] = dns_nsstatscounter_ ## counterid; \
	} while (0)

	i = 0;
	SET_NSSTATDESC(requestv4, "IPv4 requests received", "Requestv4");
	SET_NSSTATDESC(requestv6, "IPv6 requests received", "Requestv6");
	SET_NSSTATDESC(edns0in, "requests with EDNS(0) received", "ReqEdns0");
	SET_NSSTATDESC(badednsver,
		       "requests with unsupported EDNS version received",
		       "ReqBadEDNSVer");
	SET_NSSTATDESC(tsigin, "requests with TSIG received", "ReqTSIG");
	SET_NSSTATDESC(sig0in, "requests with SIG(0) received", "ReqSIG0");
	SET_NSSTATDESC(invalidsig, "requests with invalid signature",
		       "ReqBadSIG");
	SET_NSSTATDESC(tcp, "TCP requests received", "ReqTCP");
	SET_NSSTATDESC(authrej, "auth queries rejected", "AuthQryRej");
	SET_NSSTATDESC(recurserej, "recursive queries rejected", "RecQryRej");
	SET_NSSTATDESC(xfrrej, "transfer requests rejected", "XfrRej");
	SET_NSSTATDESC(updaterej, "update requests rejected", "UpdateRej");
	SET_NSSTATDESC(response, "responses sent", "Response");
	SET_NSSTATDESC(truncatedresp, "truncated responses sent",
		       "TruncatedResp");
	SET_NSSTATDESC(edns0out, "responses with EDNS(0) sent", "RespEDNS0");
	SET_NSSTATDESC(tsigout, "responses with TSIG sent", "RespTSIG");
	SET_NSSTATDESC(sig0out, "responses with SIG(0) sent", "RespSIG0");
	SET_NSSTATDESC(success, "queries resulted in successful answer",
		       "QrySuccess");
	SET_NSSTATDESC(authans, "queries resulted in authoritative answer",
		       "QryAuthAns");
	SET_NSSTATDESC(nonauthans,
		       "queries resulted in non authoritative answer",
		       "QryNoauthAns");
	SET_NSSTATDESC(referral, "queries resulted in referral answer",
		       "QryReferral");
	SET_NSSTATDESC(nxrrset, "queries resulted in nxrrset", "QryNxrrset");
	SET_NSSTATDESC(servfail, "queries resulted in SERVFAIL", "QrySERVFAIL");
	SET_NSSTATDESC(formerr, "queries resulted in FORMERR", "QryFORMERR");
	SET_NSSTATDESC(nxdomain, "queries resulted in NXDOMAIN", "QryNXDOMAIN");
	SET_NSSTATDESC(recursion, "queries caused recursion","QryRecursion");
	SET_NSSTATDESC(duplicate, "duplicate queries received", "QryDuplicate");
	SET_NSSTATDESC(dropped, "queries dropped", "QryDropped");
	SET_NSSTATDESC(failure, "other query failures", "QryFailure");
	SET_NSSTATDESC(xfrdone, "requested transfers completed", "XfrReqDone");
	SET_NSSTATDESC(updatereqfwd, "update requests forwarded",
		       "UpdateReqFwd");
	SET_NSSTATDESC(updaterespfwd, "update responses forwarded",
		       "UpdateRespFwd");
	SET_NSSTATDESC(updatefwdfail, "update forward failed", "UpdateFwdFail");
	SET_NSSTATDESC(updatedone, "updates completed", "UpdateDone");
	SET_NSSTATDESC(updatefail, "updates failed", "UpdateFail");
	SET_NSSTATDESC(updatebadprereq,
		       "updates rejected due to prerequisite failure",
		       "UpdateBadPrereq");
	INSIST(i == dns_nsstatscounter_max);

	/* Initialize resolver statistics */
	for (i = 0; i < dns_resstatscounter_max; i++)
		resstats_desc[i] = NULL;
#ifdef  HAVE_LIBXML2
	for (i = 0; i < dns_resstatscounter_max; i++)
		resstats_xmldesc[i] = NULL;
#endif

#define SET_RESSTATDESC(counterid, desc, xmldesc) \
	do { \
		set_desc(dns_resstatscounter_ ## counterid, \
			 dns_resstatscounter_max, \
			 desc, resstats_desc, xmldesc, resstats_xmldesc); \
		resstats_index[i++] = dns_resstatscounter_ ## counterid; \
	} while (0)

	i = 0;
	SET_RESSTATDESC(queryv4, "IPv4 queries sent", "Queryv4");
	SET_RESSTATDESC(queryv6, "IPv6 queries sent", "Queryv6");
	SET_RESSTATDESC(responsev4, "IPv4 responses received", "Responsev4");
	SET_RESSTATDESC(responsev6, "IPv6 responses received", "Responsev6");
	SET_RESSTATDESC(nxdomain, "NXDOMAIN received", "NXDOMAIN");
	SET_RESSTATDESC(servfail, "SERVFAIL received", "SERVFAIL");
	SET_RESSTATDESC(formerr, "FORMERR received", "FORMERR");
	SET_RESSTATDESC(othererror, "other errors received", "OtherError");
	SET_RESSTATDESC(edns0fail, "EDNS(0) query failures", "EDNS0Fail");
	SET_RESSTATDESC(mismatch, "mismatch responses received", "Mismatch");
	SET_RESSTATDESC(truncated, "truncated responses received", "Truncated");
	SET_RESSTATDESC(lame, "lame delegations received", "Lame");
	SET_RESSTATDESC(retry, "query retries", "Retry");
	SET_RESSTATDESC(dispabort, "queries aborted due to quota",
			"QueryAbort");
	SET_RESSTATDESC(dispsockfail, "failures in opening query sockets",
			"QuerySockFail");
	SET_RESSTATDESC(querytimeout, "query timeouts", "QueryTimeout");
	SET_RESSTATDESC(gluefetchv4, "IPv4 NS address fetches", "GlueFetchv4");
	SET_RESSTATDESC(gluefetchv6, "IPv6 NS address fetches", "GlueFetchv6");
	SET_RESSTATDESC(gluefetchv4fail, "IPv4 NS address fetch failed",
			"GlueFetchv4Fail");
	SET_RESSTATDESC(gluefetchv6fail, "IPv6 NS address fetch failed",
			"GlueFetchv6Fail");
	SET_RESSTATDESC(val, "DNSSEC validation attempted", "ValAttempt");
	SET_RESSTATDESC(valsuccess, "DNSSEC validation succeeded", "ValOk");
	SET_RESSTATDESC(valnegsuccess, "DNSSEC NX validation succeeded",
			"ValNegOk");
	SET_RESSTATDESC(valfail, "DNSSEC validation failed", "ValFail");
	SET_RESSTATDESC(queryrtt0, "queries with RTT < "
			DNS_RESOLVER_QRYRTTCLASS0STR "ms",
			"QryRTT" DNS_RESOLVER_QRYRTTCLASS0STR);
	SET_RESSTATDESC(queryrtt1, "queries with RTT "
			DNS_RESOLVER_QRYRTTCLASS0STR "-"
			DNS_RESOLVER_QRYRTTCLASS1STR "ms",
			"QryRTT" DNS_RESOLVER_QRYRTTCLASS1STR);
	SET_RESSTATDESC(queryrtt2, "queries with RTT "
			DNS_RESOLVER_QRYRTTCLASS1STR "-"
			DNS_RESOLVER_QRYRTTCLASS2STR "ms",
			"QryRTT" DNS_RESOLVER_QRYRTTCLASS2STR);
	SET_RESSTATDESC(queryrtt3, "queries with RTT "
			DNS_RESOLVER_QRYRTTCLASS2STR "-"
			DNS_RESOLVER_QRYRTTCLASS3STR "ms",
			"QryRTT" DNS_RESOLVER_QRYRTTCLASS3STR);
	SET_RESSTATDESC(queryrtt4, "queries with RTT "
			DNS_RESOLVER_QRYRTTCLASS3STR "-"
			DNS_RESOLVER_QRYRTTCLASS4STR "ms",
			"QryRTT" DNS_RESOLVER_QRYRTTCLASS4STR);
	SET_RESSTATDESC(queryrtt5, "queries with RTT > "
			DNS_RESOLVER_QRYRTTCLASS4STR "ms",
			"QryRTT" DNS_RESOLVER_QRYRTTCLASS4STR "+");
	INSIST(i == dns_resstatscounter_max);

	/* Initialize zone statistics */
	for (i = 0; i < dns_zonestatscounter_max; i++)
		zonestats_desc[i] = NULL;
#ifdef  HAVE_LIBXML2
	for (i = 0; i < dns_zonestatscounter_max; i++)
		zonestats_xmldesc[i] = NULL;
#endif

#define SET_ZONESTATDESC(counterid, desc, xmldesc) \
	do { \
		set_desc(dns_zonestatscounter_ ## counterid, \
			 dns_zonestatscounter_max, \
			 desc, zonestats_desc, xmldesc, zonestats_xmldesc); \
		zonestats_index[i++] = dns_zonestatscounter_ ## counterid; \
	} while (0)

	i = 0;
	SET_ZONESTATDESC(notifyoutv4, "IPv4 notifies sent", "NotifyOutv4");
	SET_ZONESTATDESC(notifyoutv6, "IPv6 notifies sent", "NotifyOutv6");
	SET_ZONESTATDESC(notifyinv4, "IPv4 notifies received", "NotifyInv4");
	SET_ZONESTATDESC(notifyinv6, "IPv6 notifies received", "NotifyInv6");
	SET_ZONESTATDESC(notifyrej, "notifies rejected", "NotifyRej");
	SET_ZONESTATDESC(soaoutv4, "IPv4 SOA queries sent", "SOAOutv4");
	SET_ZONESTATDESC(soaoutv6, "IPv6 SOA queries sent", "SOAOutv6");
	SET_ZONESTATDESC(axfrreqv4, "IPv4 AXFR requested", "AXFRReqv4");
	SET_ZONESTATDESC(axfrreqv6, "IPv6 AXFR requested", "AXFRReqv6");
	SET_ZONESTATDESC(ixfrreqv4, "IPv4 IXFR requested", "IXFRReqv4");
	SET_ZONESTATDESC(ixfrreqv6, "IPv6 IXFR requested", "IXFRReqv6");
	SET_ZONESTATDESC(xfrsuccess, "transfer requests succeeded","XfrSuccess");
	SET_ZONESTATDESC(xfrfail, "transfer requests failed", "XfrFail");
	INSIST(i == dns_zonestatscounter_max);

	/* Initialize socket statistics */
	for (i = 0; i < isc_sockstatscounter_max; i++)
		sockstats_desc[i] = NULL;
#ifdef  HAVE_LIBXML2
	for (i = 0; i < isc_sockstatscounter_max; i++)
		sockstats_xmldesc[i] = NULL;
#endif

#define SET_SOCKSTATDESC(counterid, desc, xmldesc) \
	do { \
		set_desc(isc_sockstatscounter_ ## counterid, \
			 isc_sockstatscounter_max, \
			 desc, sockstats_desc, xmldesc, sockstats_xmldesc); \
		sockstats_index[i++] = isc_sockstatscounter_ ## counterid; \
	} while (0)

	i = 0;
	SET_SOCKSTATDESC(udp4open, "UDP/IPv4 sockets opened", "UDP4Open");
	SET_SOCKSTATDESC(udp6open, "UDP/IPv6 sockets opened", "UDP6Open");
	SET_SOCKSTATDESC(tcp4open, "TCP/IPv4 sockets opened", "TCP4Open");
	SET_SOCKSTATDESC(tcp6open, "TCP/IPv6 sockets opened", "TCP6Open");
	SET_SOCKSTATDESC(unixopen, "Unix domain sockets opened", "UnixOpen");
	SET_SOCKSTATDESC(udp4openfail, "UDP/IPv4 socket open failures",
			 "UDP4OpenFail");
	SET_SOCKSTATDESC(udp6openfail, "UDP/IPv6 socket open failures",
			 "UDP6OpenFail");
	SET_SOCKSTATDESC(tcp4openfail, "TCP/IPv4 socket open failures",
			 "TCP4OpenFail");
	SET_SOCKSTATDESC(tcp6openfail, "TCP/IPv6 socket open failures",
			 "TCP6OpenFail");
	SET_SOCKSTATDESC(unixopenfail, "Unix domain socket open failures",
			 "UnixOpenFail");
	SET_SOCKSTATDESC(udp4close, "UDP/IPv4 sockets closed", "UDP4Close");
	SET_SOCKSTATDESC(udp6close, "UDP/IPv6 sockets closed", "UDP6Close");
	SET_SOCKSTATDESC(tcp4close, "TCP/IPv4 sockets closed", "TCP4Close");
	SET_SOCKSTATDESC(tcp6close, "TCP/IPv6 sockets closed", "TCP6Close");
	SET_SOCKSTATDESC(unixclose, "Unix domain sockets closed", "UnixClose");
	SET_SOCKSTATDESC(fdwatchclose, "FDwatch sockets closed",
			 "FDWatchClose");
	SET_SOCKSTATDESC(udp4bindfail, "UDP/IPv4 socket bind failures",
			 "UDP4BindFail");
	SET_SOCKSTATDESC(udp6bindfail, "UDP/IPv6 socket bind failures",
			 "UDP6BindFail");
	SET_SOCKSTATDESC(tcp4bindfail, "TCP/IPv4 socket bind failures",
			 "TCP4BindFail");
	SET_SOCKSTATDESC(tcp6bindfail, "TCP/IPv6 socket bind failures",
			 "TCP6BindFail");
	SET_SOCKSTATDESC(unixbindfail, "Unix domain socket bind failures",
			 "UnixBindFail");
	SET_SOCKSTATDESC(fdwatchbindfail, "FDwatch socket bind failures",
			 "FdwatchBindFail");
	SET_SOCKSTATDESC(udp4connectfail, "UDP/IPv4 socket connect failures",
			 "UDP4ConnFail");
	SET_SOCKSTATDESC(udp6connectfail, "UDP/IPv6 socket connect failures",
			 "UDP6ConnFail");
	SET_SOCKSTATDESC(tcp4connectfail, "TCP/IPv4 socket connect failures",
			 "TCP4ConnFail");
	SET_SOCKSTATDESC(tcp6connectfail, "TCP/IPv6 socket connect failures",
			 "TCP6ConnFail");
	SET_SOCKSTATDESC(unixconnectfail, "Unix domain socket connect failures",
			 "UnixConnFail");
	SET_SOCKSTATDESC(fdwatchconnectfail, "FDwatch socket connect failures",
			 "FDwatchConnFail");
	SET_SOCKSTATDESC(udp4connect, "UDP/IPv4 connections established",
			 "UDP4Conn");
	SET_SOCKSTATDESC(udp6connect, "UDP/IPv6 connections established",
			 "UDP6Conn");
	SET_SOCKSTATDESC(tcp4connect, "TCP/IPv4 connections established",
			 "TCP4Conn");
	SET_SOCKSTATDESC(tcp6connect, "TCP/IPv6 connections established",
			 "TCP6Conn");
	SET_SOCKSTATDESC(unixconnect, "Unix domain connections established",
			 "UnixConn");
	SET_SOCKSTATDESC(fdwatchconnect,
			 "FDwatch domain connections established",
			 "FDwatchConn");
	SET_SOCKSTATDESC(tcp4acceptfail, "TCP/IPv4 connection accept failures",
			 "TCP4AcceptFail");
	SET_SOCKSTATDESC(tcp6acceptfail, "TCP/IPv6 connection accept failures",
			 "TCP6AcceptFail");
	SET_SOCKSTATDESC(unixacceptfail,
			 "Unix domain connection accept failures",
			 "UnixAcceptFail");
	SET_SOCKSTATDESC(tcp4accept, "TCP/IPv4 connections accepted",
			 "TCP4Accept");
	SET_SOCKSTATDESC(tcp6accept, "TCP/IPv6 connections accepted",
			 "TCP6Accept");
	SET_SOCKSTATDESC(unixaccept, "Unix domain connections accepted",
			 "UnixAccept");
	SET_SOCKSTATDESC(udp4sendfail, "UDP/IPv4 send errors", "UDP4SendErr");
	SET_SOCKSTATDESC(udp6sendfail, "UDP/IPv6 send errors", "UDP6SendErr");
	SET_SOCKSTATDESC(tcp4sendfail, "TCP/IPv4 send errors", "TCP4SendErr");
	SET_SOCKSTATDESC(tcp6sendfail, "TCP/IPv6 send errors", "TCP6SendErr");
	SET_SOCKSTATDESC(unixsendfail, "Unix domain send errors",
			 "UnixSendErr");
	SET_SOCKSTATDESC(fdwatchsendfail, "FDwatch send errors",
			 "FDwatchSendErr");
	SET_SOCKSTATDESC(udp4recvfail, "UDP/IPv4 recv errors", "UDP4RecvErr");
	SET_SOCKSTATDESC(udp6recvfail, "UDP/IPv6 recv errors", "UDP6RecvErr");
	SET_SOCKSTATDESC(tcp4recvfail, "TCP/IPv4 recv errors", "TCP4RecvErr");
	SET_SOCKSTATDESC(tcp6recvfail, "TCP/IPv6 recv errors", "TCP6RecvErr");
	SET_SOCKSTATDESC(unixrecvfail, "Unix domain recv errors",
			 "UnixRecvErr");
	SET_SOCKSTATDESC(fdwatchrecvfail, "FDwatch recv errors",
			 "FDwatchRecvErr");
	INSIST(i == isc_sockstatscounter_max);

	/* Sanity check */
	for (i = 0; i < dns_nsstatscounter_max; i++)
		INSIST(nsstats_desc[i] != NULL);
	for (i = 0; i < dns_resstatscounter_max; i++)
		INSIST(resstats_desc[i] != NULL);
	for (i = 0; i < dns_zonestatscounter_max; i++)
		INSIST(zonestats_desc[i] != NULL);
	for (i = 0; i < isc_sockstatscounter_max; i++)
		INSIST(sockstats_desc[i] != NULL);
#ifdef  HAVE_LIBXML2
	for (i = 0; i < dns_nsstatscounter_max; i++)
		INSIST(nsstats_xmldesc[i] != NULL);
	for (i = 0; i < dns_resstatscounter_max; i++)
		INSIST(resstats_xmldesc[i] != NULL);
	for (i = 0; i < dns_zonestatscounter_max; i++)
		INSIST(zonestats_xmldesc[i] != NULL);
	for (i = 0; i < isc_sockstatscounter_max; i++)
		INSIST(sockstats_xmldesc[i] != NULL);
#endif
}

/*%
 * Dump callback functions.
 */
static void
generalstat_dump(isc_statscounter_t counter, isc_uint64_t val, void *arg) {
	stats_dumparg_t *dumparg = arg;

	REQUIRE(counter < dumparg->ncounters);
	dumparg->countervalues[counter] = val;
}

static isc_result_t
dump_counters(isc_stats_t *stats, statsformat_t type, void *arg,
	      const char *category, const char **desc, int ncounters,
	      int *indices, isc_uint64_t *values, int options)
{
	int i, index;
	isc_uint64_t value;
	stats_dumparg_t dumparg;
	FILE *fp;
#ifdef HAVE_LIBXML2
	xmlTextWriterPtr writer;
	int xmlrc;
#endif

#ifndef HAVE_LIBXML2
	UNUSED(category);
#endif

	dumparg.type = type;
	dumparg.ncounters = ncounters;
	dumparg.counterindices = indices;
	dumparg.countervalues = values;

	memset(values, 0, sizeof(values[0]) * ncounters);
	isc_stats_dump(stats, generalstat_dump, &dumparg, options);

	for (i = 0; i < ncounters; i++) {
		index = indices[i];
		value = values[index];

		if (value == 0 && (options & ISC_STATSDUMP_VERBOSE) == 0)
			continue;

		switch (dumparg.type) {
		case statsformat_file:
			fp = arg;
			fprintf(fp, "%20" ISC_PRINT_QUADFORMAT "u %s\n",
				value, desc[index]);
			break;
		case statsformat_xml:
#ifdef HAVE_LIBXML2
			writer = arg;

			if (category != NULL) {
				TRY0(xmlTextWriterStartElement(writer,
							       ISC_XMLCHAR
							       category));
				TRY0(xmlTextWriterStartElement(writer,
							       ISC_XMLCHAR
							       "name"));
				TRY0(xmlTextWriterWriteString(writer,
							      ISC_XMLCHAR
							      desc[index]));
				TRY0(xmlTextWriterEndElement(writer)); /* name */

				TRY0(xmlTextWriterStartElement(writer,
							       ISC_XMLCHAR
							       "counter"));
			} else {
				TRY0(xmlTextWriterStartElement(writer,
							       ISC_XMLCHAR
							       desc[index]));
			}
			TRY0(xmlTextWriterWriteFormatString(writer,
							    "%"
							    ISC_PRINT_QUADFORMAT
							    "u", value));
			TRY0(xmlTextWriterEndElement(writer)); /* counter */
			if (category != NULL)
				TRY0(xmlTextWriterEndElement(writer)); /* category */
#endif
			break;
		}
	}
	return (ISC_R_SUCCESS);
#ifdef HAVE_LIBXML2
 error:
	return (ISC_R_FAILURE);
#endif
}

static void
rdtypestat_dump(dns_rdatastatstype_t type, isc_uint64_t val, void *arg) {
	char typebuf[64];
	const char *typestr;
	stats_dumparg_t *dumparg = arg;
	FILE *fp;
#ifdef HAVE_LIBXML2
	xmlTextWriterPtr writer;
	int xmlrc;
#endif

	if ((DNS_RDATASTATSTYPE_ATTR(type) & DNS_RDATASTATSTYPE_ATTR_OTHERTYPE)
	    == 0) {
		dns_rdatatype_format(DNS_RDATASTATSTYPE_BASE(type), typebuf,
				     sizeof(typebuf));
		typestr = typebuf;
	} else
		typestr = "Others";

	switch (dumparg->type) {
	case statsformat_file:
		fp = dumparg->arg;
		fprintf(fp, "%20" ISC_PRINT_QUADFORMAT "u %s\n", val, typestr);
		break;
	case statsformat_xml:
#ifdef HAVE_LIBXML2
		writer = dumparg->arg;

		TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "rdtype"));

		TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "name"));
		TRY0(xmlTextWriterWriteString(writer, ISC_XMLCHAR typestr));
		TRY0(xmlTextWriterEndElement(writer)); /* name */

		TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "counter"));
		TRY0(xmlTextWriterWriteFormatString(writer,
					       "%" ISC_PRINT_QUADFORMAT "u",
					       val));
		TRY0(xmlTextWriterEndElement(writer)); /* counter */

		TRY0(xmlTextWriterEndElement(writer)); /* rdtype */
#endif
		break;
	}
	return;
#ifdef HAVE_LIBXML2
 error:
	dumparg->result = ISC_R_FAILURE;
	return;
#endif
}

static void
rdatasetstats_dump(dns_rdatastatstype_t type, isc_uint64_t val, void *arg) {
	stats_dumparg_t *dumparg = arg;
	FILE *fp;
	char typebuf[64];
	const char *typestr;
	isc_boolean_t nxrrset = ISC_FALSE;
#ifdef HAVE_LIBXML2
	xmlTextWriterPtr writer;
	int xmlrc;
#endif

	if ((DNS_RDATASTATSTYPE_ATTR(type) & DNS_RDATASTATSTYPE_ATTR_NXDOMAIN)
	    != 0) {
		typestr = "NXDOMAIN";
	} else if ((DNS_RDATASTATSTYPE_ATTR(type) &
		    DNS_RDATASTATSTYPE_ATTR_OTHERTYPE) != 0) {
		typestr = "Others";
	} else {
		dns_rdatatype_format(DNS_RDATASTATSTYPE_BASE(type), typebuf,
				     sizeof(typebuf));
		typestr = typebuf;
	}

	if ((DNS_RDATASTATSTYPE_ATTR(type) & DNS_RDATASTATSTYPE_ATTR_NXRRSET)
	    != 0)
		nxrrset = ISC_TRUE;

	switch (dumparg->type) {
	case statsformat_file:
		fp = dumparg->arg;
		fprintf(fp, "%20" ISC_PRINT_QUADFORMAT "u %s%s\n", val,
			nxrrset ? "!" : "", typestr);
		break;
	case statsformat_xml:
#ifdef HAVE_LIBXML2
		writer = dumparg->arg;

		TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "rrset"));
		TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "name"));
		TRY0(xmlTextWriterWriteFormatString(writer, "%s%s",
					       nxrrset ? "!" : "", typestr));
		TRY0(xmlTextWriterEndElement(writer)); /* name */

		TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "counter"));
		TRY0(xmlTextWriterWriteFormatString(writer,
					       "%" ISC_PRINT_QUADFORMAT "u",
					       val));
		TRY0(xmlTextWriterEndElement(writer)); /* counter */

		TRY0(xmlTextWriterEndElement(writer)); /* rrset */
#endif
		break;
	}
	return;
#ifdef HAVE_LIBXML2
 error:
	dumparg->result = ISC_R_FAILURE;
#endif

}

static void
opcodestat_dump(dns_opcode_t code, isc_uint64_t val, void *arg) {
	FILE *fp;
	isc_buffer_t b;
	char codebuf[64];
	stats_dumparg_t *dumparg = arg;
#ifdef HAVE_LIBXML2
	xmlTextWriterPtr writer;
	int xmlrc;
#endif

	isc_buffer_init(&b, codebuf, sizeof(codebuf) - 1);
	dns_opcode_totext(code, &b);
	codebuf[isc_buffer_usedlength(&b)] = '\0';

	switch (dumparg->type) {
	case statsformat_file:
		fp = dumparg->arg;
		fprintf(fp, "%20" ISC_PRINT_QUADFORMAT "u %s\n", val, codebuf);
		break;
	case statsformat_xml:
#ifdef HAVE_LIBXML2
		writer = dumparg->arg;

		TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "opcode"));

		TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "name"));
		TRY0(xmlTextWriterWriteString(writer, ISC_XMLCHAR codebuf));
		TRY0(xmlTextWriterEndElement(writer)); /* name */

		TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "counter"));
		TRY0(xmlTextWriterWriteFormatString(writer,
					       "%" ISC_PRINT_QUADFORMAT "u",
					       val));
		TRY0(xmlTextWriterEndElement(writer)); /* counter */

		TRY0(xmlTextWriterEndElement(writer)); /* opcode */
#endif
		break;
	}
	return;

#ifdef HAVE_LIBXML2
 error:
	dumparg->result = ISC_R_FAILURE;
	return;
#endif
}

#ifdef HAVE_LIBXML2

/* XXXMLG below here sucks. */


static isc_result_t
zone_xmlrender(dns_zone_t *zone, void *arg) {
	char buf[1024 + 32];	/* sufficiently large for zone name and class */
	dns_rdataclass_t rdclass;
	isc_uint32_t serial;
	xmlTextWriterPtr writer = arg;
	isc_stats_t *zonestats;
	isc_uint64_t nsstat_values[dns_nsstatscounter_max];
	int xmlrc;
	isc_result_t result;

	TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "zone"));

	dns_zone_name(zone, buf, sizeof(buf));
	TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "name"));
	TRY0(xmlTextWriterWriteString(writer, ISC_XMLCHAR buf));
	TRY0(xmlTextWriterEndElement(writer));

	rdclass = dns_zone_getclass(zone);
	dns_rdataclass_format(rdclass, buf, sizeof(buf));
	TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "rdataclass"));
	TRY0(xmlTextWriterWriteString(writer, ISC_XMLCHAR buf));
	TRY0(xmlTextWriterEndElement(writer));

	TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "serial"));
	if (dns_zone_getserial2(zone, &serial) == ISC_R_SUCCESS)
		TRY0(xmlTextWriterWriteFormatString(writer, "%u", serial));
	else
		TRY0(xmlTextWriterWriteString(writer, ISC_XMLCHAR "-"));
	TRY0(xmlTextWriterEndElement(writer));

	zonestats = dns_zone_getrequeststats(zone);
	if (zonestats != NULL) {
		TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "counters"));
		result = dump_counters(zonestats, statsformat_xml, writer, NULL,
				      nsstats_xmldesc, dns_nsstatscounter_max,
				      nsstats_index, nsstat_values,
				      ISC_STATSDUMP_VERBOSE);
		if (result != ISC_R_SUCCESS)
			goto error;
		TRY0(xmlTextWriterEndElement(writer)); /* counters */
	}

	TRY0(xmlTextWriterEndElement(writer)); /* zone */

	return (ISC_R_SUCCESS);
 error:
	return (ISC_R_FAILURE);
}

static isc_result_t
generatexml(ns_server_t *server, int *buflen, xmlChar **buf) {
	char boottime[sizeof "yyyy-mm-ddThh:mm:ssZ"];
	char nowstr[sizeof "yyyy-mm-ddThh:mm:ssZ"];
	isc_time_t now;
	xmlTextWriterPtr writer = NULL;
	xmlDocPtr doc = NULL;
	int xmlrc;
	dns_view_t *view;
	stats_dumparg_t dumparg;
	dns_stats_t *cachestats;
	isc_uint64_t nsstat_values[dns_nsstatscounter_max];
	isc_uint64_t resstat_values[dns_resstatscounter_max];
	isc_uint64_t zonestat_values[dns_zonestatscounter_max];
	isc_uint64_t sockstat_values[isc_sockstatscounter_max];
	isc_result_t result;

	isc_time_now(&now);
	isc_time_formatISO8601(&ns_g_boottime, boottime, sizeof boottime);
	isc_time_formatISO8601(&now, nowstr, sizeof nowstr);

	writer = xmlNewTextWriterDoc(&doc, 0);
	if (writer == NULL)
		goto error;
	TRY0(xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL));
	TRY0(xmlTextWriterWritePI(writer, ISC_XMLCHAR "xml-stylesheet",
			ISC_XMLCHAR "type=\"text/xsl\" href=\"/bind9.xsl\""));
	TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "isc"));
	TRY0(xmlTextWriterWriteAttribute(writer, ISC_XMLCHAR "version",
					 ISC_XMLCHAR "1.0"));

	TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "bind"));
	TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "statistics"));
	TRY0(xmlTextWriterWriteAttribute(writer, ISC_XMLCHAR "version",
					 ISC_XMLCHAR "2.2"));

	/* Set common fields for statistics dump */
	dumparg.type = statsformat_xml;
	dumparg.arg = writer;

	/*
	 * Start by rendering the views we know of here.  For each view we
	 * know of, call its rendering function.
	 */
	view = ISC_LIST_HEAD(server->viewlist);
	TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "views"));
	while (view != NULL) {
		TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "view"));

		TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "name"));
		TRY0(xmlTextWriterWriteString(writer, ISC_XMLCHAR view->name));
		TRY0(xmlTextWriterEndElement(writer));

		TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "zones"));
		result = dns_zt_apply(view->zonetable, ISC_TRUE, zone_xmlrender,
				      writer);
		if (result != ISC_R_SUCCESS)
			goto error;
		TRY0(xmlTextWriterEndElement(writer));

		if (view->resquerystats != NULL) {
			dumparg.result = ISC_R_SUCCESS;
			dns_rdatatypestats_dump(view->resquerystats,
						rdtypestat_dump, &dumparg, 0);
			if (dumparg.result != ISC_R_SUCCESS)
				goto error;
		}

		if (view->resstats != NULL) {
			result = dump_counters(view->resstats, statsformat_xml,
					       writer, "resstat",
					       resstats_xmldesc,
					       dns_resstatscounter_max,
					       resstats_index, resstat_values,
					       ISC_STATSDUMP_VERBOSE);
			if (result != ISC_R_SUCCESS)
				goto error;
		}

		cachestats = dns_db_getrrsetstats(view->cachedb);
		if (cachestats != NULL) {
			TRY0(xmlTextWriterStartElement(writer,
						       ISC_XMLCHAR "cache"));
			TRY0(xmlTextWriterWriteAttribute(writer,
					 ISC_XMLCHAR "name",
					 ISC_XMLCHAR
					 dns_cache_getname(view->cache)));
			dumparg.result = ISC_R_SUCCESS;
			dns_rdatasetstats_dump(cachestats, rdatasetstats_dump,
					       &dumparg, 0);
			if (dumparg.result != ISC_R_SUCCESS)
				goto error;
			TRY0(xmlTextWriterEndElement(writer)); /* cache */
		}

		TRY0(xmlTextWriterEndElement(writer)); /* view */

		view = ISC_LIST_NEXT(view, link);
	}
	TRY0(xmlTextWriterEndElement(writer)); /* views */

	TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "socketmgr"));
	isc_socketmgr_renderxml(ns_g_socketmgr, writer);
	TRY0(xmlTextWriterEndElement(writer)); /* socketmgr */

	TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "taskmgr"));
	isc_taskmgr_renderxml(ns_g_taskmgr, writer);
	TRY0(xmlTextWriterEndElement(writer)); /* taskmgr */

	TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "server"));
	TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "boot-time"));
	TRY0(xmlTextWriterWriteString(writer, ISC_XMLCHAR boottime));
	TRY0(xmlTextWriterEndElement(writer));
	TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "current-time"));
	TRY0(xmlTextWriterWriteString(writer, ISC_XMLCHAR nowstr));
	TRY0(xmlTextWriterEndElement(writer));

	TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "requests"));
	dumparg.result = ISC_R_SUCCESS;
	dns_opcodestats_dump(server->opcodestats, opcodestat_dump, &dumparg,
			     0);
	if (dumparg.result != ISC_R_SUCCESS)
		goto error;
	TRY0(xmlTextWriterEndElement(writer)); /* requests */

	TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "queries-in"));
	dumparg.result = ISC_R_SUCCESS;
	dns_rdatatypestats_dump(server->rcvquerystats, rdtypestat_dump,
				&dumparg, 0);
	if (dumparg.result != ISC_R_SUCCESS)
		goto error;
	TRY0(xmlTextWriterEndElement(writer)); /* queries-in */

	result = dump_counters(server->nsstats, statsformat_xml, writer,
			       "nsstat", nsstats_xmldesc,
				dns_nsstatscounter_max,
				nsstats_index, nsstat_values,
				ISC_STATSDUMP_VERBOSE);
	if (result != ISC_R_SUCCESS)
		goto error;

	result = dump_counters(server->zonestats, statsformat_xml, writer,
			       "zonestat", zonestats_xmldesc,
			       dns_zonestatscounter_max, zonestats_index,
			       zonestat_values, ISC_STATSDUMP_VERBOSE);
	if (result != ISC_R_SUCCESS)
		goto error;

	/*
	 * Most of the common resolver statistics entries are 0, so we don't
	 * use the verbose dump here.
	 */
	result = dump_counters(server->resolverstats, statsformat_xml, writer,
			       "resstat", resstats_xmldesc,
			       dns_resstatscounter_max, resstats_index,
			       resstat_values, 0);
	if (result != ISC_R_SUCCESS)
		goto error;

	result = dump_counters(server->sockstats, statsformat_xml, writer,
			       "sockstat", sockstats_xmldesc,
			       isc_sockstatscounter_max, sockstats_index,
			       sockstat_values, ISC_STATSDUMP_VERBOSE);
	if (result != ISC_R_SUCCESS)
		goto error;

	TRY0(xmlTextWriterEndElement(writer)); /* server */

	TRY0(xmlTextWriterStartElement(writer, ISC_XMLCHAR "memory"));
	isc_mem_renderxml(writer);
	TRY0(xmlTextWriterEndElement(writer)); /* memory */

	TRY0(xmlTextWriterEndElement(writer)); /* statistics */
	TRY0(xmlTextWriterEndElement(writer)); /* bind */
	TRY0(xmlTextWriterEndElement(writer)); /* isc */

	TRY0(xmlTextWriterEndDocument(writer));

	xmlFreeTextWriter(writer);

	xmlDocDumpFormatMemoryEnc(doc, buf, buflen, "UTF-8", 1);
	xmlFreeDoc(doc);
	return (ISC_R_SUCCESS);

 error:
	if (writer != NULL)
		xmlFreeTextWriter(writer);
	if (doc != NULL)
		xmlFreeDoc(doc);
	return (ISC_R_FAILURE);
}

static void
wrap_xmlfree(isc_buffer_t *buffer, void *arg) {
	UNUSED(arg);

	xmlFree(isc_buffer_base(buffer));
}

static isc_result_t
render_index(const char *url, const char *querystring, void *arg,
	     unsigned int *retcode, const char **retmsg, const char **mimetype,
	     isc_buffer_t *b, isc_httpdfree_t **freecb,
	     void **freecb_args)
{
	unsigned char *msg;
	int msglen;
	ns_server_t *server = arg;
	isc_result_t result;

	UNUSED(url);
	UNUSED(querystring);

	result = generatexml(server, &msglen, &msg);

	if (result == ISC_R_SUCCESS) {
		*retcode = 200;
		*retmsg = "OK";
		*mimetype = "text/xml";
		isc_buffer_reinit(b, msg, msglen);
		isc_buffer_add(b, msglen);
		*freecb = wrap_xmlfree;
		*freecb_args = NULL;
	}

	return (result);
}

#endif	/* HAVE_LIBXML2 */

static isc_result_t
render_xsl(const char *url, const char *querystring, void *args,
	   unsigned int *retcode, const char **retmsg, const char **mimetype,
	   isc_buffer_t *b, isc_httpdfree_t **freecb,
	   void **freecb_args)
{
	UNUSED(url);
	UNUSED(querystring);
	UNUSED(args);

	*retcode = 200;
	*retmsg = "OK";
	*mimetype = "text/xslt+xml";
	isc_buffer_reinit(b, xslmsg, strlen(xslmsg));
	isc_buffer_add(b, strlen(xslmsg));
	*freecb = NULL;
	*freecb_args = NULL;

	return (ISC_R_SUCCESS);
}

static void
shutdown_listener(ns_statschannel_t *listener) {
	char socktext[ISC_SOCKADDR_FORMATSIZE];
	isc_sockaddr_format(&listener->address, socktext, sizeof(socktext));
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,NS_LOGMODULE_SERVER,
		      ISC_LOG_NOTICE, "stopping statistics channel on %s",
		      socktext);

	isc_httpdmgr_shutdown(&listener->httpdmgr);
}

static isc_boolean_t
client_ok(const isc_sockaddr_t *fromaddr, void *arg) {
	ns_statschannel_t *listener = arg;
	isc_netaddr_t netaddr;
	char socktext[ISC_SOCKADDR_FORMATSIZE];
	int match;

	REQUIRE(listener != NULL);

	isc_netaddr_fromsockaddr(&netaddr, fromaddr);

	LOCK(&listener->lock);
	if (dns_acl_match(&netaddr, NULL, listener->acl, &ns_g_server->aclenv,
			  &match, NULL) == ISC_R_SUCCESS && match > 0) {
		UNLOCK(&listener->lock);
		return (ISC_TRUE);
	}
	UNLOCK(&listener->lock);

	isc_sockaddr_format(fromaddr, socktext, sizeof(socktext));
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_SERVER, ISC_LOG_WARNING,
		      "rejected statistics connection from %s", socktext);

	return (ISC_FALSE);
}

static void
destroy_listener(void *arg) {
	ns_statschannel_t *listener = arg;

	REQUIRE(listener != NULL);
	REQUIRE(!ISC_LINK_LINKED(listener, link));

	/* We don't have to acquire the lock here since it's already unlinked */
	dns_acl_detach(&listener->acl);

	DESTROYLOCK(&listener->lock);
	isc_mem_putanddetach(&listener->mctx, listener, sizeof(*listener));
}

static isc_result_t
add_listener(ns_server_t *server, ns_statschannel_t **listenerp,
	     const cfg_obj_t *listen_params, const cfg_obj_t *config,
	     isc_sockaddr_t *addr, cfg_aclconfctx_t *aclconfctx,
	     const char *socktext)
{
	isc_result_t result;
	ns_statschannel_t *listener;
	isc_task_t *task = NULL;
	isc_socket_t *sock = NULL;
	const cfg_obj_t *allow;
	dns_acl_t *new_acl = NULL;

	listener = isc_mem_get(server->mctx, sizeof(*listener));
	if (listener == NULL)
		return (ISC_R_NOMEMORY);

	listener->httpdmgr = NULL;
	listener->address = *addr;
	listener->acl = NULL;
	listener->mctx = NULL;
	ISC_LINK_INIT(listener, link);

	result = isc_mutex_init(&listener->lock);
	if (result != ISC_R_SUCCESS) {
		isc_mem_put(server->mctx, listener, sizeof(*listener));
		return (ISC_R_FAILURE);
	}

	isc_mem_attach(server->mctx, &listener->mctx);

	allow = cfg_tuple_get(listen_params, "allow");
	if (allow != NULL && cfg_obj_islist(allow)) {
		result = cfg_acl_fromconfig(allow, config, ns_g_lctx,
					    aclconfctx, listener->mctx, 0,
					    &new_acl);
	} else
		result = dns_acl_any(listener->mctx, &new_acl);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
	dns_acl_attach(new_acl, &listener->acl);
	dns_acl_detach(&new_acl);

	result = isc_task_create(ns_g_taskmgr, 0, &task);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
	isc_task_setname(task, "statchannel", NULL);

	result = isc_socket_create(ns_g_socketmgr, isc_sockaddr_pf(addr),
				   isc_sockettype_tcp, &sock);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
	isc_socket_setname(sock, "statchannel", NULL);

#ifndef ISC_ALLOW_MAPPED
	isc_socket_ipv6only(sock, ISC_TRUE);
#endif

	result = isc_socket_bind(sock, addr, ISC_SOCKET_REUSEADDRESS);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = isc_httpdmgr_create(server->mctx, sock, task, client_ok,
				     destroy_listener, listener, ns_g_timermgr,
				     &listener->httpdmgr);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

#ifdef HAVE_LIBXML2
	isc_httpdmgr_addurl(listener->httpdmgr, "/", render_index, server);
#endif
	isc_httpdmgr_addurl(listener->httpdmgr, "/bind9.xsl", render_xsl,
			    server);

	*listenerp = listener;
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_SERVER, ISC_LOG_NOTICE,
		      "statistics channel listening on %s", socktext);

cleanup:
	if (result != ISC_R_SUCCESS) {
		if (listener->acl != NULL)
			dns_acl_detach(&listener->acl);
		DESTROYLOCK(&listener->lock);
		isc_mem_putanddetach(&listener->mctx, listener,
				     sizeof(*listener));
	}
	if (task != NULL)
		isc_task_detach(&task);
	if (sock != NULL)
		isc_socket_detach(&sock);

	return (result);
}

static void
update_listener(ns_server_t *server, ns_statschannel_t **listenerp,
		const cfg_obj_t *listen_params, const cfg_obj_t *config,
		isc_sockaddr_t *addr, cfg_aclconfctx_t *aclconfctx,
		const char *socktext)
{
	ns_statschannel_t *listener;
	const cfg_obj_t *allow = NULL;
	dns_acl_t *new_acl = NULL;
	isc_result_t result = ISC_R_SUCCESS;

	for (listener = ISC_LIST_HEAD(server->statschannels);
	     listener != NULL;
	     listener = ISC_LIST_NEXT(listener, link))
		if (isc_sockaddr_equal(addr, &listener->address))
			break;

	if (listener == NULL) {
		*listenerp = NULL;
		return;
	}

	/*
	 * Now, keep the old access list unless a new one can be made.
	 */
	allow = cfg_tuple_get(listen_params, "allow");
	if (allow != NULL && cfg_obj_islist(allow)) {
		result = cfg_acl_fromconfig(allow, config, ns_g_lctx,
					    aclconfctx, listener->mctx, 0,
					    &new_acl);
	} else
		result = dns_acl_any(listener->mctx, &new_acl);

	if (result == ISC_R_SUCCESS) {
		LOCK(&listener->lock);

		dns_acl_detach(&listener->acl);
		dns_acl_attach(new_acl, &listener->acl);
		dns_acl_detach(&new_acl);

		UNLOCK(&listener->lock);
	} else {
		cfg_obj_log(listen_params, ns_g_lctx, ISC_LOG_WARNING,
			    "couldn't install new acl for "
			    "statistics channel %s: %s",
			    socktext, isc_result_totext(result));
	}

	*listenerp = listener;
}

isc_result_t
ns_statschannels_configure(ns_server_t *server, const cfg_obj_t *config,
			 cfg_aclconfctx_t *aclconfctx)
{
	ns_statschannel_t *listener, *listener_next;
	ns_statschannellist_t new_listeners;
	const cfg_obj_t *statschannellist = NULL;
	const cfg_listelt_t *element, *element2;
	char socktext[ISC_SOCKADDR_FORMATSIZE];

	RUNTIME_CHECK(isc_once_do(&once, init_desc) == ISC_R_SUCCESS);

	ISC_LIST_INIT(new_listeners);

	/*
	 * Get the list of named.conf 'statistics-channels' statements.
	 */
	(void)cfg_map_get(config, "statistics-channels", &statschannellist);

	/*
	 * Run through the new address/port list, noting sockets that are
	 * already being listened on and moving them to the new list.
	 *
	 * Identifying duplicate addr/port combinations is left to either
	 * the underlying config code, or to the bind attempt getting an
	 * address-in-use error.
	 */
	if (statschannellist != NULL) {
#ifndef HAVE_LIBXML2
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_WARNING,
			      "statistics-channels specified but not effective "
			      "due to missing XML library");
#endif

		for (element = cfg_list_first(statschannellist);
		     element != NULL;
		     element = cfg_list_next(element)) {
			const cfg_obj_t *statschannel;
			const cfg_obj_t *listenercfg = NULL;

			statschannel = cfg_listelt_value(element);
			(void)cfg_map_get(statschannel, "inet",
					  &listenercfg);
			if (listenercfg == NULL)
				continue;

			for (element2 = cfg_list_first(listenercfg);
			     element2 != NULL;
			     element2 = cfg_list_next(element2)) {
				const cfg_obj_t *listen_params;
				const cfg_obj_t *obj;
				isc_sockaddr_t addr;

				listen_params = cfg_listelt_value(element2);

				obj = cfg_tuple_get(listen_params, "address");
				addr = *cfg_obj_assockaddr(obj);
				if (isc_sockaddr_getport(&addr) == 0)
					isc_sockaddr_setport(&addr, NS_STATSCHANNEL_HTTPPORT);

				isc_sockaddr_format(&addr, socktext,
						    sizeof(socktext));

				isc_log_write(ns_g_lctx,
					      NS_LOGCATEGORY_GENERAL,
					      NS_LOGMODULE_SERVER,
					      ISC_LOG_DEBUG(9),
					      "processing statistics "
					      "channel %s",
					      socktext);

				update_listener(server, &listener,
						listen_params, config, &addr,
						aclconfctx, socktext);

				if (listener != NULL) {
					/*
					 * Remove the listener from the old
					 * list, so it won't be shut down.
					 */
					ISC_LIST_UNLINK(server->statschannels,
							listener, link);
				} else {
					/*
					 * This is a new listener.
					 */
					isc_result_t r;

					r = add_listener(server, &listener,
							 listen_params, config,
							 &addr, aclconfctx,
							 socktext);
					if (r != ISC_R_SUCCESS) {
						cfg_obj_log(listen_params,
							    ns_g_lctx,
							    ISC_LOG_WARNING,
							    "couldn't allocate "
							    "statistics channel"
							    " %s: %s",
							    socktext,
							    isc_result_totext(r));
					}
				}

				if (listener != NULL)
					ISC_LIST_APPEND(new_listeners, listener,
							link);
			}
		}
	}

	for (listener = ISC_LIST_HEAD(server->statschannels);
	     listener != NULL;
	     listener = listener_next) {
		listener_next = ISC_LIST_NEXT(listener, link);
		ISC_LIST_UNLINK(server->statschannels, listener, link);
		shutdown_listener(listener);
	}

	ISC_LIST_APPENDLIST(server->statschannels, new_listeners, link);
	return (ISC_R_SUCCESS);
}

void
ns_statschannels_shutdown(ns_server_t *server) {
	ns_statschannel_t *listener;

	while ((listener = ISC_LIST_HEAD(server->statschannels)) != NULL) {
		ISC_LIST_UNLINK(server->statschannels, listener, link);
		shutdown_listener(listener);
	}
}

isc_result_t
ns_stats_dump(ns_server_t *server, FILE *fp) {
	isc_stdtime_t now;
	isc_result_t result;
	dns_view_t *view;
	dns_zone_t *zone, *next;
	stats_dumparg_t dumparg;
	isc_uint64_t nsstat_values[dns_nsstatscounter_max];
	isc_uint64_t resstat_values[dns_resstatscounter_max];
	isc_uint64_t zonestat_values[dns_zonestatscounter_max];
	isc_uint64_t sockstat_values[isc_sockstatscounter_max];

	RUNTIME_CHECK(isc_once_do(&once, init_desc) == ISC_R_SUCCESS);

	/* Set common fields */
	dumparg.type = statsformat_file;
	dumparg.arg = fp;

	isc_stdtime_get(&now);
	fprintf(fp, "+++ Statistics Dump +++ (%lu)\n", (unsigned long)now);

	fprintf(fp, "++ Incoming Requests ++\n");
	dns_opcodestats_dump(server->opcodestats, opcodestat_dump, &dumparg, 0);

	fprintf(fp, "++ Incoming Queries ++\n");
	dns_rdatatypestats_dump(server->rcvquerystats, rdtypestat_dump,
				&dumparg, 0);

	fprintf(fp, "++ Outgoing Queries ++\n");
	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link)) {
		if (view->resquerystats == NULL)
			continue;
		if (strcmp(view->name, "_default") == 0)
			fprintf(fp, "[View: default]\n");
		else
			fprintf(fp, "[View: %s]\n", view->name);
		dns_rdatatypestats_dump(view->resquerystats, rdtypestat_dump,
					&dumparg, 0);
	}

	fprintf(fp, "++ Name Server Statistics ++\n");
	(void) dump_counters(server->nsstats, statsformat_file, fp, NULL,
			     nsstats_desc, dns_nsstatscounter_max,
			     nsstats_index, nsstat_values, 0);

	fprintf(fp, "++ Zone Maintenance Statistics ++\n");
	(void) dump_counters(server->zonestats, statsformat_file, fp, NULL,
			     zonestats_desc, dns_zonestatscounter_max,
			     zonestats_index, zonestat_values, 0);

	fprintf(fp, "++ Resolver Statistics ++\n");
	fprintf(fp, "[Common]\n");
	(void) dump_counters(server->resolverstats, statsformat_file, fp, NULL,
			     resstats_desc, dns_resstatscounter_max,
			     resstats_index, resstat_values, 0);
	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link)) {
		if (view->resstats == NULL)
			continue;
		if (strcmp(view->name, "_default") == 0)
			fprintf(fp, "[View: default]\n");
		else
			fprintf(fp, "[View: %s]\n", view->name);
		(void) dump_counters(view->resstats, statsformat_file, fp, NULL,
				     resstats_desc, dns_resstatscounter_max,
				     resstats_index, resstat_values, 0);
	}

	fprintf(fp, "++ Cache DB RRsets ++\n");
	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link)) {
		dns_stats_t *cachestats;

		cachestats = dns_db_getrrsetstats(view->cachedb);
		if (cachestats == NULL)
			continue;
		if (strcmp(view->name, "_default") == 0)
			fprintf(fp, "[View: default]\n");
		else
			fprintf(fp, "[View: %s (Cache: %s)]\n", view->name,
				dns_cache_getname(view->cache));
		if (dns_view_iscacheshared(view)) {
			/*
			 * Avoid dumping redundant statistics when the cache is
			 * shared.
			 */
			continue;
		}
		dns_rdatasetstats_dump(cachestats, rdatasetstats_dump, &dumparg,
				       0);
	}

	fprintf(fp, "++ Socket I/O Statistics ++\n");
	(void) dump_counters(server->sockstats, statsformat_file, fp, NULL,
			     sockstats_desc, isc_sockstatscounter_max,
			     sockstats_index, sockstat_values, 0);

	fprintf(fp, "++ Per Zone Query Statistics ++\n");
	zone = NULL;
	for (result = dns_zone_first(server->zonemgr, &zone);
	     result == ISC_R_SUCCESS;
	     next = NULL, result = dns_zone_next(zone, &next), zone = next)
	{
		isc_stats_t *zonestats = dns_zone_getrequeststats(zone);
		if (zonestats != NULL) {
			char zonename[DNS_NAME_FORMATSIZE];

			dns_name_format(dns_zone_getorigin(zone),
					zonename, sizeof(zonename));
			view = dns_zone_getview(zone);

			fprintf(fp, "[%s", zonename);
			if (strcmp(view->name, "_default") != 0)
				fprintf(fp, " (view: %s)", view->name);
			fprintf(fp, "]\n");

			(void) dump_counters(zonestats, statsformat_file, fp,
					     NULL, nsstats_desc,
					     dns_nsstatscounter_max,
					     nsstats_index, nsstat_values, 0);
		}
	}

	fprintf(fp, "--- Statistics Dump --- (%lu)\n", (unsigned long)now);

	return (ISC_R_SUCCESS);	/* this function currently always succeeds */
}
