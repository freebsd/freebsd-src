/*
 * Copyright (C) 2004-2010  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000-2002  Internet Software Consortium.
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

/* $Id: check-tool.c,v 1.41 2010-09-07 23:46:59 tbox Exp $ */

/*! \file */

#include <config.h>

#include <stdio.h>

#ifdef _WIN32
#include <Winsock2.h>
#endif

#include "check-tool.h"
#include <isc/buffer.h>
#include <isc/log.h>
#include <isc/mem.h>
#include <isc/netdb.h>
#include <isc/net.h>
#include <isc/region.h>
#include <isc/stdio.h>
#include <isc/string.h>
#include <isc/symtab.h>
#include <isc/types.h>
#include <isc/util.h>

#include <dns/fixedname.h>
#include <dns/log.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdataset.h>
#include <dns/types.h>
#include <dns/zone.h>

#include <isccfg/log.h>

#ifndef CHECK_SIBLING
#define CHECK_SIBLING 1
#endif

#ifndef CHECK_LOCAL
#define CHECK_LOCAL 1
#endif

#ifdef HAVE_ADDRINFO
#ifdef HAVE_GETADDRINFO
#ifdef HAVE_GAISTRERROR
#define USE_GETADDRINFO
#endif
#endif
#endif

#define CHECK(r) \
	do { \
		result = (r); \
		if (result != ISC_R_SUCCESS) \
			goto cleanup; \
	} while (0)

#define ERR_IS_CNAME 1
#define ERR_NO_ADDRESSES 2
#define ERR_LOOKUP_FAILURE 3
#define ERR_EXTRA_A 4
#define ERR_EXTRA_AAAA 5
#define ERR_MISSING_GLUE 5
#define ERR_IS_MXCNAME 6
#define ERR_IS_SRVCNAME 7

static const char *dbtype[] = { "rbt" };

int debug = 0;
isc_boolean_t nomerge = ISC_TRUE;
#if CHECK_LOCAL
isc_boolean_t docheckmx = ISC_TRUE;
isc_boolean_t dochecksrv = ISC_TRUE;
isc_boolean_t docheckns = ISC_TRUE;
#else
isc_boolean_t docheckmx = ISC_FALSE;
isc_boolean_t dochecksrv = ISC_FALSE;
isc_boolean_t docheckns = ISC_FALSE;
#endif
unsigned int zone_options = DNS_ZONEOPT_CHECKNS |
			    DNS_ZONEOPT_CHECKMX |
			    DNS_ZONEOPT_MANYERRORS |
			    DNS_ZONEOPT_CHECKNAMES |
			    DNS_ZONEOPT_CHECKINTEGRITY |
#if CHECK_SIBLING
			    DNS_ZONEOPT_CHECKSIBLING |
#endif
			    DNS_ZONEOPT_CHECKWILDCARD |
			    DNS_ZONEOPT_WARNMXCNAME |
			    DNS_ZONEOPT_WARNSRVCNAME;

/*
 * This needs to match the list in bin/named/log.c.
 */
static isc_logcategory_t categories[] = {
	{ "",		     0 },
	{ "client",	     0 },
	{ "network",	     0 },
	{ "update",	     0 },
	{ "queries",	     0 },
	{ "unmatched", 	     0 },
	{ "update-security", 0 },
	{ "query-errors",    0 },
	{ NULL,		     0 }
};

static isc_symtab_t *symtab = NULL;
static isc_mem_t *sym_mctx;

static void
freekey(char *key, unsigned int type, isc_symvalue_t value, void *userarg) {
	UNUSED(type);
	UNUSED(value);
	isc_mem_free(userarg, key);
}

static void
add(char *key, int value) {
	isc_result_t result;
	isc_symvalue_t symvalue;

	if (sym_mctx == NULL) {
		result = isc_mem_create(0, 0, &sym_mctx);
		if (result != ISC_R_SUCCESS)
			return;
	}

	if (symtab == NULL) {
		result = isc_symtab_create(sym_mctx, 100, freekey, sym_mctx,
					   ISC_FALSE, &symtab);
		if (result != ISC_R_SUCCESS)
			return;
	}

	key = isc_mem_strdup(sym_mctx, key);
	if (key == NULL)
		return;

	symvalue.as_pointer = NULL;
	result = isc_symtab_define(symtab, key, value, symvalue,
				   isc_symexists_reject);
	if (result != ISC_R_SUCCESS)
		isc_mem_free(sym_mctx, key);
}

static isc_boolean_t
logged(char *key, int value) {
	isc_result_t result;

	if (symtab == NULL)
		return (ISC_FALSE);

	result = isc_symtab_lookup(symtab, key, value, NULL);
	if (result == ISC_R_SUCCESS)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

static isc_boolean_t
checkns(dns_zone_t *zone, dns_name_t *name, dns_name_t *owner,
	dns_rdataset_t *a, dns_rdataset_t *aaaa)
{
#ifdef USE_GETADDRINFO
	dns_rdataset_t *rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	struct addrinfo hints, *ai, *cur;
	char namebuf[DNS_NAME_FORMATSIZE + 1];
	char ownerbuf[DNS_NAME_FORMATSIZE];
	char addrbuf[sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:123.123.123.123")];
	isc_boolean_t answer = ISC_TRUE;
	isc_boolean_t match;
	const char *type;
	void *ptr = NULL;
	int result;

	REQUIRE(a == NULL || !dns_rdataset_isassociated(a) ||
		a->type == dns_rdatatype_a);
	REQUIRE(aaaa == NULL || !dns_rdataset_isassociated(aaaa) ||
		aaaa->type == dns_rdatatype_aaaa);
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	dns_name_format(name, namebuf, sizeof(namebuf) - 1);
	/*
	 * Turn off search.
	 */
	if (dns_name_countlabels(name) > 1U)
		strcat(namebuf, ".");
	dns_name_format(owner, ownerbuf, sizeof(ownerbuf));

	result = getaddrinfo(namebuf, NULL, &hints, &ai);
	dns_name_format(name, namebuf, sizeof(namebuf) - 1);
	switch (result) {
	case 0:
		/*
		 * Work around broken getaddrinfo() implementations that
		 * fail to set ai_canonname on first entry.
		 */
		cur = ai;
		while (cur != NULL && cur->ai_canonname == NULL &&
		       cur->ai_next != NULL)
			cur = cur->ai_next;
		if (cur != NULL && cur->ai_canonname != NULL &&
		    strcasecmp(cur->ai_canonname, namebuf) != 0 &&
		    !logged(namebuf, ERR_IS_CNAME)) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "%s/NS '%s' (out of zone) "
				     "is a CNAME '%s' (illegal)",
				     ownerbuf, namebuf,
				     cur->ai_canonname);
			/* XXX950 make fatal for 9.5.0 */
			/* answer = ISC_FALSE; */
			add(namebuf, ERR_IS_CNAME);
		}
		break;
	case EAI_NONAME:
#if defined(EAI_NODATA) && (EAI_NODATA != EAI_NONAME)
	case EAI_NODATA:
#endif
		if (!logged(namebuf, ERR_NO_ADDRESSES)) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "%s/NS '%s' (out of zone) "
				     "has no addresses records (A or AAAA)",
				     ownerbuf, namebuf);
			add(namebuf, ERR_NO_ADDRESSES);
		}
		/* XXX950 make fatal for 9.5.0 */
		return (ISC_TRUE);

	default:
		if (!logged(namebuf, ERR_LOOKUP_FAILURE)) {
			dns_zone_log(zone, ISC_LOG_WARNING,
				     "getaddrinfo(%s) failed: %s",
				     namebuf, gai_strerror(result));
			add(namebuf, ERR_LOOKUP_FAILURE);
		}
		return (ISC_TRUE);
	}
	if (a == NULL || aaaa == NULL)
		return (answer);
	/*
	 * Check that all glue records really exist.
	 */
	if (!dns_rdataset_isassociated(a))
		goto checkaaaa;
	result = dns_rdataset_first(a);
	while (result == ISC_R_SUCCESS) {
		dns_rdataset_current(a, &rdata);
		match = ISC_FALSE;
		for (cur = ai; cur != NULL; cur = cur->ai_next) {
			if (cur->ai_family != AF_INET)
				continue;
			ptr = &((struct sockaddr_in *)(cur->ai_addr))->sin_addr;
			if (memcmp(ptr, rdata.data, rdata.length) == 0) {
				match = ISC_TRUE;
				break;
			}
		}
		if (!match && !logged(namebuf, ERR_EXTRA_A)) {
			dns_zone_log(zone, ISC_LOG_ERROR, "%s/NS '%s' "
				     "extra GLUE A record (%s)",
				     ownerbuf, namebuf,
				     inet_ntop(AF_INET, rdata.data,
					       addrbuf, sizeof(addrbuf)));
			add(namebuf, ERR_EXTRA_A);
			/* XXX950 make fatal for 9.5.0 */
			/* answer = ISC_FALSE; */
		}
		dns_rdata_reset(&rdata);
		result = dns_rdataset_next(a);
	}

 checkaaaa:
	if (!dns_rdataset_isassociated(aaaa))
		goto checkmissing;
	result = dns_rdataset_first(aaaa);
	while (result == ISC_R_SUCCESS) {
		dns_rdataset_current(aaaa, &rdata);
		match = ISC_FALSE;
		for (cur = ai; cur != NULL; cur = cur->ai_next) {
			if (cur->ai_family != AF_INET6)
				continue;
			ptr = &((struct sockaddr_in6 *)(cur->ai_addr))->sin6_addr;
			if (memcmp(ptr, rdata.data, rdata.length) == 0) {
				match = ISC_TRUE;
				break;
			}
		}
		if (!match && !logged(namebuf, ERR_EXTRA_AAAA)) {
			dns_zone_log(zone, ISC_LOG_ERROR, "%s/NS '%s' "
				     "extra GLUE AAAA record (%s)",
				     ownerbuf, namebuf,
				     inet_ntop(AF_INET6, rdata.data,
					       addrbuf, sizeof(addrbuf)));
			add(namebuf, ERR_EXTRA_AAAA);
			/* XXX950 make fatal for 9.5.0. */
			/* answer = ISC_FALSE; */
		}
		dns_rdata_reset(&rdata);
		result = dns_rdataset_next(aaaa);
	}

 checkmissing:
	/*
	 * Check that all addresses appear in the glue.
	 */
	if (!logged(namebuf, ERR_MISSING_GLUE)) {
		isc_boolean_t missing_glue = ISC_FALSE;
		for (cur = ai; cur != NULL; cur = cur->ai_next) {
			switch (cur->ai_family) {
			case AF_INET:
				rdataset = a;
				ptr = &((struct sockaddr_in *)(cur->ai_addr))->sin_addr;
				type = "A";
				break;
			case AF_INET6:
				rdataset = aaaa;
				ptr = &((struct sockaddr_in6 *)(cur->ai_addr))->sin6_addr;
				type = "AAAA";
				break;
			default:
				 continue;
			}
			match = ISC_FALSE;
			if (dns_rdataset_isassociated(rdataset))
				result = dns_rdataset_first(rdataset);
			else
				result = ISC_R_FAILURE;
			while (result == ISC_R_SUCCESS && !match) {
				dns_rdataset_current(rdataset, &rdata);
				if (memcmp(ptr, rdata.data, rdata.length) == 0)
					match = ISC_TRUE;
				dns_rdata_reset(&rdata);
				result = dns_rdataset_next(rdataset);
			}
			if (!match) {
				dns_zone_log(zone, ISC_LOG_ERROR, "%s/NS '%s' "
					     "missing GLUE %s record (%s)",
					     ownerbuf, namebuf, type,
					     inet_ntop(cur->ai_family, ptr,
						       addrbuf, sizeof(addrbuf)));
				/* XXX950 make fatal for 9.5.0. */
				/* answer = ISC_FALSE; */
				missing_glue = ISC_TRUE;
			}
		}
		if (missing_glue)
			add(namebuf, ERR_MISSING_GLUE);
	}
	freeaddrinfo(ai);
	return (answer);
#else
	return (ISC_TRUE);
#endif
}

static isc_boolean_t
checkmx(dns_zone_t *zone, dns_name_t *name, dns_name_t *owner) {
#ifdef USE_GETADDRINFO
	struct addrinfo hints, *ai, *cur;
	char namebuf[DNS_NAME_FORMATSIZE + 1];
	char ownerbuf[DNS_NAME_FORMATSIZE];
	int result;
	int level = ISC_LOG_ERROR;
	isc_boolean_t answer = ISC_TRUE;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	dns_name_format(name, namebuf, sizeof(namebuf) - 1);
	/*
	 * Turn off search.
	 */
	if (dns_name_countlabels(name) > 1U)
		strcat(namebuf, ".");
	dns_name_format(owner, ownerbuf, sizeof(ownerbuf));

	result = getaddrinfo(namebuf, NULL, &hints, &ai);
	dns_name_format(name, namebuf, sizeof(namebuf) - 1);
	switch (result) {
	case 0:
		/*
		 * Work around broken getaddrinfo() implementations that
		 * fail to set ai_canonname on first entry.
		 */
		cur = ai;
		while (cur != NULL && cur->ai_canonname == NULL &&
		       cur->ai_next != NULL)
			cur = cur->ai_next;
		if (cur != NULL && cur->ai_canonname != NULL &&
		    strcasecmp(cur->ai_canonname, namebuf) != 0) {
			if ((zone_options & DNS_ZONEOPT_WARNMXCNAME) != 0)
				level = ISC_LOG_WARNING;
			if ((zone_options & DNS_ZONEOPT_IGNOREMXCNAME) == 0) {
				if (!logged(namebuf, ERR_IS_MXCNAME)) {
					dns_zone_log(zone, level,
						     "%s/MX '%s' (out of zone)"
						     " is a CNAME '%s' "
						     "(illegal)",
						     ownerbuf, namebuf,
						     cur->ai_canonname);
					add(namebuf, ERR_IS_MXCNAME);
				}
				if (level == ISC_LOG_ERROR)
					answer = ISC_FALSE;
			}
		}
		freeaddrinfo(ai);
		return (answer);

	case EAI_NONAME:
#if defined(EAI_NODATA) && (EAI_NODATA != EAI_NONAME)
	case EAI_NODATA:
#endif
		if (!logged(namebuf, ERR_NO_ADDRESSES)) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "%s/MX '%s' (out of zone) "
				     "has no addresses records (A or AAAA)",
				     ownerbuf, namebuf);
			add(namebuf, ERR_NO_ADDRESSES);
		}
		/* XXX950 make fatal for 9.5.0. */
		return (ISC_TRUE);

	default:
		if (!logged(namebuf, ERR_LOOKUP_FAILURE)) {
			dns_zone_log(zone, ISC_LOG_WARNING,
			     "getaddrinfo(%s) failed: %s",
			     namebuf, gai_strerror(result));
			add(namebuf, ERR_LOOKUP_FAILURE);
		}
		return (ISC_TRUE);
	}
#else
	return (ISC_TRUE);
#endif
}

static isc_boolean_t
checksrv(dns_zone_t *zone, dns_name_t *name, dns_name_t *owner) {
#ifdef USE_GETADDRINFO
	struct addrinfo hints, *ai, *cur;
	char namebuf[DNS_NAME_FORMATSIZE + 1];
	char ownerbuf[DNS_NAME_FORMATSIZE];
	int result;
	int level = ISC_LOG_ERROR;
	isc_boolean_t answer = ISC_TRUE;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	dns_name_format(name, namebuf, sizeof(namebuf) - 1);
	/*
	 * Turn off search.
	 */
	if (dns_name_countlabels(name) > 1U)
		strcat(namebuf, ".");
	dns_name_format(owner, ownerbuf, sizeof(ownerbuf));

	result = getaddrinfo(namebuf, NULL, &hints, &ai);
	dns_name_format(name, namebuf, sizeof(namebuf) - 1);
	switch (result) {
	case 0:
		/*
		 * Work around broken getaddrinfo() implementations that
		 * fail to set ai_canonname on first entry.
		 */
		cur = ai;
		while (cur != NULL && cur->ai_canonname == NULL &&
		       cur->ai_next != NULL)
			cur = cur->ai_next;
		if (cur != NULL && cur->ai_canonname != NULL &&
		    strcasecmp(cur->ai_canonname, namebuf) != 0) {
			if ((zone_options & DNS_ZONEOPT_WARNSRVCNAME) != 0)
				level = ISC_LOG_WARNING;
			if ((zone_options & DNS_ZONEOPT_IGNORESRVCNAME) == 0) {
				if (!logged(namebuf, ERR_IS_SRVCNAME)) {
					dns_zone_log(zone, level, "%s/SRV '%s'"
						     " (out of zone) is a "
						     "CNAME '%s' (illegal)",
						     ownerbuf, namebuf,
						     cur->ai_canonname);
					add(namebuf, ERR_IS_SRVCNAME);
				}
				if (level == ISC_LOG_ERROR)
					answer = ISC_FALSE;
			}
		}
		freeaddrinfo(ai);
		return (answer);

	case EAI_NONAME:
#if defined(EAI_NODATA) && (EAI_NODATA != EAI_NONAME)
	case EAI_NODATA:
#endif
		if (!logged(namebuf, ERR_NO_ADDRESSES)) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "%s/SRV '%s' (out of zone) "
				     "has no addresses records (A or AAAA)",
				     ownerbuf, namebuf);
			add(namebuf, ERR_NO_ADDRESSES);
		}
		/* XXX950 make fatal for 9.5.0. */
		return (ISC_TRUE);

	default:
		if (!logged(namebuf, ERR_LOOKUP_FAILURE)) {
			dns_zone_log(zone, ISC_LOG_WARNING,
				     "getaddrinfo(%s) failed: %s",
				     namebuf, gai_strerror(result));
			add(namebuf, ERR_LOOKUP_FAILURE);
		}
		return (ISC_TRUE);
	}
#else
	return (ISC_TRUE);
#endif
}

isc_result_t
setup_logging(isc_mem_t *mctx, FILE *errout, isc_log_t **logp) {
	isc_logdestination_t destination;
	isc_logconfig_t *logconfig = NULL;
	isc_log_t *log = NULL;

	RUNTIME_CHECK(isc_log_create(mctx, &log, &logconfig) == ISC_R_SUCCESS);
	isc_log_registercategories(log, categories);
	isc_log_setcontext(log);
	dns_log_init(log);
	dns_log_setcontext(log);
	cfg_log_init(log);

	destination.file.stream = errout;
	destination.file.name = NULL;
	destination.file.versions = ISC_LOG_ROLLNEVER;
	destination.file.maximum_size = 0;
	RUNTIME_CHECK(isc_log_createchannel(logconfig, "stderr",
				       ISC_LOG_TOFILEDESC,
				       ISC_LOG_DYNAMIC,
				       &destination, 0) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_log_usechannel(logconfig, "stderr",
					 NULL, NULL) == ISC_R_SUCCESS);

	*logp = log;
	return (ISC_R_SUCCESS);
}

/*% load the zone */
isc_result_t
load_zone(isc_mem_t *mctx, const char *zonename, const char *filename,
	  dns_masterformat_t fileformat, const char *classname,
	  dns_zone_t **zonep)
{
	isc_result_t result;
	dns_rdataclass_t rdclass;
	isc_textregion_t region;
	isc_buffer_t buffer;
	dns_fixedname_t fixorigin;
	dns_name_t *origin;
	dns_zone_t *zone = NULL;

	REQUIRE(zonep == NULL || *zonep == NULL);

	if (debug)
		fprintf(stderr, "loading \"%s\" from \"%s\" class \"%s\"\n",
			zonename, filename, classname);

	CHECK(dns_zone_create(&zone, mctx));

	dns_zone_settype(zone, dns_zone_master);

	isc_buffer_init(&buffer, zonename, strlen(zonename));
	isc_buffer_add(&buffer, strlen(zonename));
	dns_fixedname_init(&fixorigin);
	origin = dns_fixedname_name(&fixorigin);
	CHECK(dns_name_fromtext(origin, &buffer, dns_rootname, 0, NULL));
	CHECK(dns_zone_setorigin(zone, origin));
	CHECK(dns_zone_setdbtype(zone, 1, (const char * const *) dbtype));
	CHECK(dns_zone_setfile2(zone, filename, fileformat));

	DE_CONST(classname, region.base);
	region.length = strlen(classname);
	CHECK(dns_rdataclass_fromtext(&rdclass, &region));

	dns_zone_setclass(zone, rdclass);
	dns_zone_setoption(zone, zone_options, ISC_TRUE);
	dns_zone_setoption(zone, DNS_ZONEOPT_NOMERGE, nomerge);
	if (docheckmx)
		dns_zone_setcheckmx(zone, checkmx);
	if (docheckns)
		dns_zone_setcheckns(zone, checkns);
	if (dochecksrv)
		dns_zone_setchecksrv(zone, checksrv);

	CHECK(dns_zone_load(zone));
	if (zonep != NULL) {
		*zonep = zone;
		zone = NULL;
	}

 cleanup:
	if (zone != NULL)
		dns_zone_detach(&zone);
	return (result);
}

/*% dump the zone */
isc_result_t
dump_zone(const char *zonename, dns_zone_t *zone, const char *filename,
	  dns_masterformat_t fileformat, const dns_master_style_t *style)
{
	isc_result_t result;
	FILE *output = stdout;

	if (debug) {
		if (filename != NULL && strcmp(filename, "-") != 0)
			fprintf(stderr, "dumping \"%s\" to \"%s\"\n",
				zonename, filename);
		else
			fprintf(stderr, "dumping \"%s\"\n", zonename);
	}

	if (filename != NULL && strcmp(filename, "-") != 0) {
		result = isc_stdio_open(filename, "w+", &output);

		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "could not open output "
				"file \"%s\" for writing\n", filename);
			return (ISC_R_FAILURE);
		}
	}

	result = dns_zone_dumptostream2(zone, output, fileformat, style);

	if (output != stdout)
		(void)isc_stdio_close(output);

	return (result);
}

#ifdef _WIN32
void
InitSockets(void) {
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	wVersionRequested = MAKEWORD(2, 0);

	err = WSAStartup( wVersionRequested, &wsaData );
	if (err != 0) {
		fprintf(stderr, "WSAStartup() failed: %d\n", err);
		exit(1);
	}
}

void
DestroySockets(void) {
	WSACleanup();
}
#endif

