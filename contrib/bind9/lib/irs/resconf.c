/*
 * Copyright (C) 2009, 2011, 2012  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id$ */

/*! \file resconf.c */

/**
 * Module for parsing resolv.conf files (largely derived from lwconfig.c).
 *
 *    irs_resconf_load() opens the file filename and parses it to initialize
 *    the configuration structure.
 *
 * \section lwconfig_return Return Values
 *
 *    irs_resconf_load() returns #IRS_R_SUCCESS if it successfully read and
 *    parsed filename. It returns a non-0 error code if filename could not be
 *    opened or contained incorrect resolver statements.
 *
 * \section lwconfig_see See Also
 *
 *    stdio(3), \link resolver resolver \endlink
 *
 * \section files Files
 *
 *    /etc/resolv.conf
 */

#include <config.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <isc/magic.h>
#include <isc/mem.h>
#include <isc/netaddr.h>
#include <isc/sockaddr.h>
#include <isc/util.h>

#include <irs/resconf.h>

#define IRS_RESCONF_MAGIC		ISC_MAGIC('R', 'E', 'S', 'c')
#define IRS_RESCONF_VALID(c)		ISC_MAGIC_VALID(c, IRS_RESCONF_MAGIC)

/*!
 * protocol constants
 */

#if ! defined(NS_INADDRSZ)
#define NS_INADDRSZ	 4
#endif

#if ! defined(NS_IN6ADDRSZ)
#define NS_IN6ADDRSZ	16
#endif

/*!
 * resolv.conf parameters
 */

#define RESCONFMAXNAMESERVERS 3		/*%< max 3 "nameserver" entries */
#define RESCONFMAXSEARCH 8		/*%< max 8 domains in "search" entry */
#define RESCONFMAXLINELEN 256		/*%< max size of a line */
#define RESCONFMAXSORTLIST 10		/*%< max 10 */

/*!
 * configuration data structure
 */

struct irs_resconf {
	/*
	 * The configuration data is a thread-specific object, and does not
	 * need to be locked.
	 */
	unsigned int		magic;
	isc_mem_t		*mctx;

	isc_sockaddrlist_t	nameservers;
	unsigned int		numns; /*%< number of configured servers */

	char	       		*domainname;
	char 	       		*search[RESCONFMAXSEARCH];
	isc_uint8_t		searchnxt; /*%< index for next free slot */

	irs_resconf_searchlist_t searchlist;

	struct {
		isc_netaddr_t	addr;
		/*% mask has a non-zero 'family' if set */
		isc_netaddr_t	mask;
	} sortlist[RESCONFMAXSORTLIST];
	isc_uint8_t		sortlistnxt;

	/*%< non-zero if 'options debug' set */
	isc_uint8_t		resdebug;
	/*%< set to n in 'options ndots:n' */
	isc_uint8_t		ndots;
};

static isc_result_t
resconf_parsenameserver(irs_resconf_t *conf,  FILE *fp);
static isc_result_t
resconf_parsedomain(irs_resconf_t *conf,  FILE *fp);
static isc_result_t
resconf_parsesearch(irs_resconf_t *conf,  FILE *fp);
static isc_result_t
resconf_parsesortlist(irs_resconf_t *conf,  FILE *fp);
static isc_result_t
resconf_parseoption(irs_resconf_t *ctx,  FILE *fp);

/*!
 * Eat characters from FP until EOL or EOF. Returns EOF or '\n'
 */
static int
eatline(FILE *fp) {
	int ch;

	ch = fgetc(fp);
	while (ch != '\n' && ch != EOF)
		ch = fgetc(fp);

	return (ch);
}

/*!
 * Eats white space up to next newline or non-whitespace character (of
 * EOF). Returns the last character read. Comments are considered white
 * space.
 */
static int
eatwhite(FILE *fp) {
	int ch;

	ch = fgetc(fp);
	while (ch != '\n' && ch != EOF && isspace((unsigned char)ch))
		ch = fgetc(fp);

	if (ch == ';' || ch == '#')
		ch = eatline(fp);

	return (ch);
}

/*!
 * Skip over any leading whitespace and then read in the next sequence of
 * non-whitespace characters. In this context newline is not considered
 * whitespace. Returns EOF on end-of-file, or the character
 * that caused the reading to stop.
 */
static int
getword(FILE *fp, char *buffer, size_t size) {
	int ch;
	char *p = buffer;

	REQUIRE(buffer != NULL);
	REQUIRE(size > 0U);

	*p = '\0';

	ch = eatwhite(fp);

	if (ch == EOF)
		return (EOF);

	do {
		*p = '\0';

		if (ch == EOF || isspace((unsigned char)ch))
			break;
		else if ((size_t) (p - buffer) == size - 1)
			return (EOF);	/* Not enough space. */

		*p++ = (char)ch;
		ch = fgetc(fp);
	} while (1);

	return (ch);
}

static isc_result_t
add_server(isc_mem_t *mctx, const char *address_str,
	   isc_sockaddrlist_t *nameservers)
{
	int error;
	isc_sockaddr_t *address = NULL;
	struct addrinfo hints, *res;
	isc_result_t result = ISC_R_SUCCESS;

	res = NULL;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_NUMERICHOST;
	error = getaddrinfo(address_str, "53", &hints, &res);
	if (error != 0)
		return (ISC_R_BADADDRESSFORM);

	/* XXX: special case: treat all-0 IPv4 address as loopback */
	if (res->ai_family == AF_INET) {
		struct in_addr *v4;
		unsigned char zeroaddress[] = {0, 0, 0, 0};
		unsigned char loopaddress[] = {127, 0, 0, 1};

		v4 = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
		if (memcmp(v4, zeroaddress, 4) == 0)
			memcpy(v4, loopaddress, 4);
	}

	address = isc_mem_get(mctx, sizeof(*address));
	if (address == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}
	if (res->ai_addrlen > sizeof(address->type)) {
		isc_mem_put(mctx, address, sizeof(*address));
		result = ISC_R_RANGE;
		goto cleanup;
	}
	address->length = res->ai_addrlen;
	memcpy(&address->type.ss, res->ai_addr, res->ai_addrlen);
	ISC_LINK_INIT(address, link);
	ISC_LIST_APPEND(*nameservers, address, link);

  cleanup:
	freeaddrinfo(res);

	return (result);
}

static isc_result_t
create_addr(const char *buffer, isc_netaddr_t *addr, int convert_zero) {
	struct in_addr v4;
	struct in6_addr v6;

	if (inet_aton(buffer, &v4) == 1) {
		if (convert_zero) {
			unsigned char zeroaddress[] = {0, 0, 0, 0};
			unsigned char loopaddress[] = {127, 0, 0, 1};
			if (memcmp(&v4, zeroaddress, 4) == 0)
				memcpy(&v4, loopaddress, 4);
		}
		addr->family = AF_INET;
		memcpy(&addr->type.in, &v4, NS_INADDRSZ);
		addr->zone = 0;
	} else if (inet_pton(AF_INET6, buffer, &v6) == 1) {
		addr->family = AF_INET6;
		memcpy(&addr->type.in6, &v6, NS_IN6ADDRSZ);
		addr->zone = 0;
	} else
		return (ISC_R_BADADDRESSFORM); /* Unrecognised format. */

	return (ISC_R_SUCCESS);
}

static isc_result_t
resconf_parsenameserver(irs_resconf_t *conf,  FILE *fp) {
	char word[RESCONFMAXLINELEN];
	int cp;
	isc_result_t result;

	if (conf->numns == RESCONFMAXNAMESERVERS)
		return (ISC_R_SUCCESS);

	cp = getword(fp, word, sizeof(word));
	if (strlen(word) == 0U)
		return (ISC_R_UNEXPECTEDEND); /* Nothing on line. */
	else if (cp == ' ' || cp == '\t')
		cp = eatwhite(fp);

	if (cp != EOF && cp != '\n')
		return (ISC_R_UNEXPECTEDTOKEN); /* Extra junk on line. */

	result = add_server(conf->mctx, word, &conf->nameservers);
	if (result != ISC_R_SUCCESS)
		return (result);
	conf->numns++;

	return (ISC_R_SUCCESS);
}

static isc_result_t
resconf_parsedomain(irs_resconf_t *conf,  FILE *fp) {
	char word[RESCONFMAXLINELEN];
	int res, i;

	res = getword(fp, word, sizeof(word));
	if (strlen(word) == 0U)
		return (ISC_R_UNEXPECTEDEND); /* Nothing else on line. */
	else if (res == ' ' || res == '\t')
		res = eatwhite(fp);

	if (res != EOF && res != '\n')
		return (ISC_R_UNEXPECTEDTOKEN); /* Extra junk on line. */

	if (conf->domainname != NULL)
		isc_mem_free(conf->mctx, conf->domainname);

	/*
	 * Search and domain are mutually exclusive.
	 */
	for (i = 0; i < RESCONFMAXSEARCH; i++) {
		if (conf->search[i] != NULL) {
			isc_mem_free(conf->mctx, conf->search[i]);
			conf->search[i] = NULL;
		}
	}
	conf->searchnxt = 0;

	conf->domainname = isc_mem_strdup(conf->mctx, word);
	if (conf->domainname == NULL)
		return (ISC_R_NOMEMORY);

	return (ISC_R_SUCCESS);
}

static isc_result_t
resconf_parsesearch(irs_resconf_t *conf,  FILE *fp) {
	int idx, delim;
	char word[RESCONFMAXLINELEN];

	if (conf->domainname != NULL) {
		/*
		 * Search and domain are mutually exclusive.
		 */
		isc_mem_free(conf->mctx, conf->domainname);
		conf->domainname = NULL;
	}

	/*
	 * Remove any previous search definitions.
	 */
	for (idx = 0; idx < RESCONFMAXSEARCH; idx++) {
		if (conf->search[idx] != NULL) {
			isc_mem_free(conf->mctx, conf->search[idx]);
			conf->search[idx] = NULL;
		}
	}
	conf->searchnxt = 0;

	delim = getword(fp, word, sizeof(word));
	if (strlen(word) == 0U)
		return (ISC_R_UNEXPECTEDEND); /* Nothing else on line. */

	idx = 0;
	while (strlen(word) > 0U) {
		if (conf->searchnxt == RESCONFMAXSEARCH)
			goto ignore; /* Too many domains. */

		conf->search[idx] = isc_mem_strdup(conf->mctx, word);
		if (conf->search[idx] == NULL)
			return (ISC_R_NOMEMORY);
		idx++;
		conf->searchnxt++;

	ignore:
		if (delim == EOF || delim == '\n')
			break;
		else
			delim = getword(fp, word, sizeof(word));
	}

	return (ISC_R_SUCCESS);
}

static isc_result_t
resconf_parsesortlist(irs_resconf_t *conf,  FILE *fp) {
	int delim, res, idx;
	char word[RESCONFMAXLINELEN];
	char *p;

	delim = getword(fp, word, sizeof(word));
	if (strlen(word) == 0U)
		return (ISC_R_UNEXPECTEDEND); /* Empty line after keyword. */

	while (strlen(word) > 0U) {
		if (conf->sortlistnxt == RESCONFMAXSORTLIST)
			return (ISC_R_QUOTA); /* Too many values. */

		p = strchr(word, '/');
		if (p != NULL)
			*p++ = '\0';

		idx = conf->sortlistnxt;
		res = create_addr(word, &conf->sortlist[idx].addr, 1);
		if (res != ISC_R_SUCCESS)
			return (res);

		if (p != NULL) {
			res = create_addr(p, &conf->sortlist[idx].mask, 0);
			if (res != ISC_R_SUCCESS)
				return (res);
		} else {
			/*
			 * Make up a mask. (XXX: is this correct?)
			 */
			conf->sortlist[idx].mask = conf->sortlist[idx].addr;
			memset(&conf->sortlist[idx].mask.type, 0xff,
			       sizeof(conf->sortlist[idx].mask.type));
		}

		conf->sortlistnxt++;

		if (delim == EOF || delim == '\n')
			break;
		else
			delim = getword(fp, word, sizeof(word));
	}

	return (ISC_R_SUCCESS);
}

static isc_result_t
resconf_parseoption(irs_resconf_t *conf,  FILE *fp) {
	int delim;
	long ndots;
	char *p;
	char word[RESCONFMAXLINELEN];

	delim = getword(fp, word, sizeof(word));
	if (strlen(word) == 0U)
		return (ISC_R_UNEXPECTEDEND); /* Empty line after keyword. */

	while (strlen(word) > 0U) {
		if (strcmp("debug", word) == 0) {
			conf->resdebug = 1;
		} else if (strncmp("ndots:", word, 6) == 0) {
			ndots = strtol(word + 6, &p, 10);
			if (*p != '\0') /* Bad string. */
				return (ISC_R_UNEXPECTEDTOKEN);
			if (ndots < 0 || ndots > 0xff) /* Out of range. */
				return (ISC_R_RANGE);
			conf->ndots = (isc_uint8_t)ndots;
		}

		if (delim == EOF || delim == '\n')
			break;
		else
			delim = getword(fp, word, sizeof(word));
	}

	return (ISC_R_SUCCESS);
}

static isc_result_t
add_search(irs_resconf_t *conf, char *domain) {
	irs_resconf_search_t *entry;

	entry = isc_mem_get(conf->mctx, sizeof(*entry));
	if (entry == NULL)
		return (ISC_R_NOMEMORY);

	entry->domain = domain;
	ISC_LINK_INIT(entry, link);
	ISC_LIST_APPEND(conf->searchlist, entry, link);

	return (ISC_R_SUCCESS);
}

/*% parses a file and fills in the data structure. */
isc_result_t
irs_resconf_load(isc_mem_t *mctx, const char *filename, irs_resconf_t **confp)
{
	FILE *fp = NULL;
	char word[256];
	isc_result_t rval, ret;
	irs_resconf_t *conf;
	int i, stopchar;

	REQUIRE(mctx != NULL);
	REQUIRE(filename != NULL);
	REQUIRE(strlen(filename) > 0U);
	REQUIRE(confp != NULL && *confp == NULL);

	conf = isc_mem_get(mctx, sizeof(*conf));
	if (conf == NULL)
		return (ISC_R_NOMEMORY);

	conf->mctx = mctx;
	ISC_LIST_INIT(conf->nameservers);
	conf->numns = 0;
	conf->domainname = NULL;
	conf->searchnxt = 0;
	conf->resdebug = 0;
	conf->ndots = 1;
	for (i = 0; i < RESCONFMAXSEARCH; i++)
		conf->search[i] = NULL;

	errno = 0;
	if ((fp = fopen(filename, "r")) == NULL) {
		isc_mem_put(mctx, conf, sizeof(*conf));
		return (ISC_R_INVALIDFILE);
	}

	ret = ISC_R_SUCCESS;
	do {
		stopchar = getword(fp, word, sizeof(word));
		if (stopchar == EOF) {
			rval = ISC_R_SUCCESS;
			POST(rval);
			break;
		}

		if (strlen(word) == 0U)
			rval = ISC_R_SUCCESS;
		else if (strcmp(word, "nameserver") == 0)
			rval = resconf_parsenameserver(conf, fp);
		else if (strcmp(word, "domain") == 0)
			rval = resconf_parsedomain(conf, fp);
		else if (strcmp(word, "search") == 0)
			rval = resconf_parsesearch(conf, fp);
		else if (strcmp(word, "sortlist") == 0)
			rval = resconf_parsesortlist(conf, fp);
		else if (strcmp(word, "options") == 0)
			rval = resconf_parseoption(conf, fp);
		else {
			/* unrecognised word. Ignore entire line */
			rval = ISC_R_SUCCESS;
			stopchar = eatline(fp);
			if (stopchar == EOF) {
				break;
			}
		}
		if (ret == ISC_R_SUCCESS && rval != ISC_R_SUCCESS)
			ret = rval;
	} while (1);

	fclose(fp);

	/* If we don't find a nameserver fall back to localhost */
	if (conf->numns == 0) {
		INSIST(ISC_LIST_EMPTY(conf->nameservers));

		/* XXX: should we catch errors? */
		(void)add_server(conf->mctx, "127.0.0.1", &conf->nameservers);
		(void)add_server(conf->mctx, "::1", &conf->nameservers);
	}

	/*
	 * Construct unified search list from domain or configured
	 * search list
	 */
	ISC_LIST_INIT(conf->searchlist);
	if (conf->domainname != NULL) {
		ret = add_search(conf, conf->domainname);
	} else if (conf->searchnxt > 0) {
		for (i = 0; i < conf->searchnxt; i++) {
			ret = add_search(conf, conf->search[i]);
			if (ret != ISC_R_SUCCESS)
				break;
		}
	}

	conf->magic = IRS_RESCONF_MAGIC;

	if (ret != ISC_R_SUCCESS)
		irs_resconf_destroy(&conf);
	else
		*confp = conf;

	return (ret);
}

void
irs_resconf_destroy(irs_resconf_t **confp) {
	irs_resconf_t *conf;
	isc_sockaddr_t *address;
	irs_resconf_search_t *searchentry;
	int i;

	REQUIRE(confp != NULL);
	conf = *confp;
	REQUIRE(IRS_RESCONF_VALID(conf));

	while ((searchentry = ISC_LIST_HEAD(conf->searchlist)) != NULL) {
		ISC_LIST_UNLINK(conf->searchlist, searchentry, link);
		isc_mem_put(conf->mctx, searchentry, sizeof(*searchentry));
	}

	while ((address = ISC_LIST_HEAD(conf->nameservers)) != NULL) {
		ISC_LIST_UNLINK(conf->nameservers, address, link);
		isc_mem_put(conf->mctx, address, sizeof(*address));
	}

	if (conf->domainname != NULL)
		isc_mem_free(conf->mctx, conf->domainname);

	for (i = 0; i < RESCONFMAXSEARCH; i++) {
		if (conf->search[i] != NULL)
			isc_mem_free(conf->mctx, conf->search[i]);
	}

	isc_mem_put(conf->mctx, conf, sizeof(*conf));

	*confp = NULL;
}

isc_sockaddrlist_t *
irs_resconf_getnameservers(irs_resconf_t *conf) {
	REQUIRE(IRS_RESCONF_VALID(conf));

	return (&conf->nameservers);
}

irs_resconf_searchlist_t *
irs_resconf_getsearchlist(irs_resconf_t *conf) {
	REQUIRE(IRS_RESCONF_VALID(conf));

	return (&conf->searchlist);
}

unsigned int
irs_resconf_getndots(irs_resconf_t *conf) {
	REQUIRE(IRS_RESCONF_VALID(conf));

	return ((unsigned int)conf->ndots);
}
