/*
 * Copyright (C) 2004, 2005, 2007, 2009-2012  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2001-2003  Internet Software Consortium.
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

/* $Id: builtin.c,v 1.20.14.3 2012/01/11 20:19:40 ckb Exp $ */

/*! \file
 * \brief
 * The built-in "version", "hostname", "id", "authors" and "empty" databases.
 */

#include <config.h>

#include <string.h>
#include <stdio.h>

#include <isc/mem.h>
#include <isc/print.h>
#include <isc/result.h>
#include <isc/util.h>

#include <dns/result.h>
#include <dns/sdb.h>

#include <named/builtin.h>
#include <named/globals.h>
#include <named/server.h>
#include <named/os.h>

typedef struct builtin builtin_t;

static isc_result_t do_version_lookup(dns_sdblookup_t *lookup);
static isc_result_t do_hostname_lookup(dns_sdblookup_t *lookup);
static isc_result_t do_authors_lookup(dns_sdblookup_t *lookup);
static isc_result_t do_id_lookup(dns_sdblookup_t *lookup);
static isc_result_t do_empty_lookup(dns_sdblookup_t *lookup);
static isc_result_t do_dns64_lookup(dns_sdblookup_t *lookup);

/*
 * We can't use function pointers as the db_data directly
 * because ANSI C does not guarantee that function pointers
 * can safely be cast to void pointers and back.
 */

struct builtin {
	isc_result_t (*do_lookup)(dns_sdblookup_t *lookup);
	char *server;
	char *contact;
};

static builtin_t version_builtin = { do_version_lookup,  NULL, NULL };
static builtin_t hostname_builtin = { do_hostname_lookup, NULL, NULL };
static builtin_t authors_builtin = { do_authors_lookup, NULL, NULL };
static builtin_t id_builtin = { do_id_lookup, NULL, NULL };
static builtin_t empty_builtin = { do_empty_lookup, NULL, NULL };
static builtin_t dns64_builtin = { do_dns64_lookup, NULL, NULL };

static dns_sdbimplementation_t *builtin_impl;
static dns_sdbimplementation_t *dns64_impl;

/*
 * Pre computed HEX * 16 or 1 table.
 */
static const unsigned char hex16[256] = {
	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, /*00*/
	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,	/*10*/
	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, /*20*/
	 0, 16, 32, 48, 64, 80, 96,112,128,144,  1,  1,  1,  1,  1,  1,	/*30*/
	 1,160,176,192,208,224,240,  1,  1,  1,  1,  1,  1,  1,  1,  1, /*40*/
	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, /*50*/
	 1,160,176,192,208,224,240,  1,  1,  1,  1,  1,  1,  1,  1,  1, /*60*/
	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, /*70*/
	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, /*80*/
	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, /*90*/
	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, /*A0*/
	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, /*B0*/
	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, /*C0*/
	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, /*D0*/
	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, /*E0*/
	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1  /*F0*/
};

const unsigned char decimal[] = "0123456789";

static size_t
dns64_rdata(unsigned char *v, size_t start, unsigned char *rdata) {
	size_t i, j = 0;

	for (i = 0; i < 4U; i++) {
		unsigned char c = v[start++];
		if (start == 7U)
			start++;
		if (c > 99) {
			rdata[j++] = 3;
			rdata[j++] = decimal[c/100]; c = c % 100;
			rdata[j++] = decimal[c/10]; c = c % 10;
			rdata[j++] = decimal[c];
		} else if (c > 9) {
			rdata[j++] = 2;
			rdata[j++] = decimal[c/10]; c = c % 10;
			rdata[j++] = decimal[c];
		} else {
			rdata[j++] = 1;
			rdata[j++] = decimal[c];
		}
	}
	memcpy(&rdata[j], "\07in-addr\04arpa", 14);
	return (j + 14);
}

static isc_result_t
dns64_cname(const dns_name_t *zone, const dns_name_t *name,
	    dns_sdblookup_t *lookup)
{
	size_t zlen, nlen, j, len;
	unsigned char v[16], n;
	unsigned int i;
	unsigned char rdata[sizeof("123.123.123.123.in-addr.arpa.")];
	unsigned char *ndata;

	/*
	 * The combined length of the zone and name is 74.
	 *
	 * The minimum zone length is 10 ((3)ip6(4)arpa(0)).
	 *
	 * The length of name should always be even as we are expecting
	 * a series of nibbles.
	 */
	zlen = zone->length;
	nlen = name->length;
	if ((zlen + nlen) > 74U || zlen < 10U || (nlen % 2) != 0U)
		return (ISC_R_NOTFOUND);

	/*
	 * We assume the zone name is well formed.
	 */

	/*
	 * XXXMPA We could check the dns64 suffix here if we need to.
	 */
	/*
	 * Check that name is a series of nibbles.
	 * Compute the byte values that correspond to the nibbles as we go.
	 *
	 * Shift the final result 4 bits, by setting 'i' to 1, if we if we
	 * have a odd number of nibbles so that "must be zero" tests below
	 * are byte aligned and we correctly return ISC_R_NOTFOUND or
	 * ISC_R_SUCCESS.  We will not generate a CNAME in this case.
	 */
	ndata = name->ndata;
	i = (nlen % 4) == 2U ? 1 : 0;
	j = nlen;
	memset(v, 0, sizeof(v));
	while (j != 0U) {
		INSIST((i/2) < sizeof(v));
		if (ndata[0] != 1)
			return (ISC_R_NOTFOUND);
		n = hex16[ndata[1]&0xff];
		if (n == 1)
			return (ISC_R_NOTFOUND);
		v[i/2] = n | (v[i/2]>>4);
		j -= 2;
		ndata += 2;
		i++;
	}

	/*
	 * If we get here then we know name only consisted of nibbles.
	 * Now we need to determine if the name exists or not and whether
	 * it corresponds to a empty node in the zone or there should be
	 * a CNAME.
	 */
#define ZLEN(x) (10 + (x)/2)
	switch (zlen) {
	case ZLEN(32):	/* prefix len 32 */
		/*
		 * The nibbles that map to this byte must be zero for 'name'
		 * to exist in the zone.
		 */
		if (nlen > 16U && v[(nlen-1)/4 - 4] != 0)
			return (ISC_R_NOTFOUND);
		/*
		 * If the total length is not 74 then this is a empty node
		 * so return success.
		 */
		if (nlen + zlen != 74U)
			return (ISC_R_SUCCESS);
		len = dns64_rdata(v, 8, rdata);
		break;
	case ZLEN(40):	/* prefix len 40 */
		/*
		 * The nibbles that map to this byte must be zero for 'name'
		 * to exist in the zone.
		 */
		if (nlen > 12U && v[(nlen-1)/4 - 3] != 0)
			return (ISC_R_NOTFOUND);
		/*
		 * If the total length is not 74 then this is a empty node
		 * so return success.
		 */
		if (nlen + zlen != 74U)
			return (ISC_R_SUCCESS);
		len = dns64_rdata(v, 6, rdata);
		break;
	case ZLEN(48):	/* prefix len 48 */
		/*
		 * The nibbles that map to this byte must be zero for 'name'
		 * to exist in the zone.
		 */
		if (nlen > 8U && v[(nlen-1)/4 - 2] != 0)
			return (ISC_R_NOTFOUND);
		/*
		 * If the total length is not 74 then this is a empty node
		 * so return success.
		 */
		if (nlen + zlen != 74U)
			return (ISC_R_SUCCESS);
		len = dns64_rdata(v, 5, rdata);
		break;
	case ZLEN(56):	/* prefix len 56 */
		/*
		 * The nibbles that map to this byte must be zero for 'name'
		 * to exist in the zone.
		 */
		if (nlen > 4U && v[(nlen-1)/4 - 1] != 0)
			return (ISC_R_NOTFOUND);
		/*
		 * If the total length is not 74 then this is a empty node
		 * so return success.
		 */
		if (nlen + zlen != 74U)
			return (ISC_R_SUCCESS);
		len = dns64_rdata(v, 4, rdata);
		break;
	case ZLEN(64):	/* prefix len 64 */
		/*
		 * The nibbles that map to this byte must be zero for 'name'
		 * to exist in the zone.
		 */
		if (v[(nlen-1)/4] != 0)
			return (ISC_R_NOTFOUND);
		/*
		 * If the total length is not 74 then this is a empty node
		 * so return success.
		 */
		if (nlen + zlen != 74U)
			return (ISC_R_SUCCESS);
		len = dns64_rdata(v, 3, rdata);
		break;
	case ZLEN(96):	/* prefix len 96 */
		/*
		 * If the total length is not 74 then this is a empty node
		 * so return success.
		 */
		if (nlen + zlen != 74U)
			return (ISC_R_SUCCESS);
		len = dns64_rdata(v, 0, rdata);
		break;
	default:
		/*
		 * This should never be reached unless someone adds a
		 * zone declaration with this internal type to named.conf.
		 */
		return (ISC_R_NOTFOUND);
	}
	return (dns_sdb_putrdata(lookup, dns_rdatatype_cname, 600, rdata, len));
}

static isc_result_t
builtin_lookup(const char *zone, const char *name, void *dbdata,
	       dns_sdblookup_t *lookup)
{
	builtin_t *b = (builtin_t *) dbdata;

	UNUSED(zone);

	if (strcmp(name, "@") == 0)
		return (b->do_lookup(lookup));
	else
		return (ISC_R_NOTFOUND);
}

static isc_result_t
dns64_lookup(const dns_name_t *zone, const dns_name_t *name, void *dbdata,
	     dns_sdblookup_t *lookup)
{
	builtin_t *b = (builtin_t *) dbdata;

	if (name->labels == 0 && name->length == 0)
		return (b->do_lookup(lookup));
	else
		return (dns64_cname(zone, name, lookup));
}

static isc_result_t
put_txt(dns_sdblookup_t *lookup, const char *text) {
	unsigned char buf[256];
	unsigned int len = strlen(text);
	if (len > 255)
		len = 255; /* Silently truncate */
	buf[0] = len;
	memcpy(&buf[1], text, len);
	return (dns_sdb_putrdata(lookup, dns_rdatatype_txt, 0, buf, len + 1));
}

static isc_result_t
do_version_lookup(dns_sdblookup_t *lookup) {
	if (ns_g_server->version_set) {
		if (ns_g_server->version == NULL)
			return (ISC_R_SUCCESS);
		else
			return (put_txt(lookup, ns_g_server->version));
	} else {
		return (put_txt(lookup, ns_g_version));
	}
}

static isc_result_t
do_hostname_lookup(dns_sdblookup_t *lookup) {
	if (ns_g_server->hostname_set) {
		if (ns_g_server->hostname == NULL)
			return (ISC_R_SUCCESS);
		else
			return (put_txt(lookup, ns_g_server->hostname));
	} else {
		char buf[256];
		isc_result_t result = ns_os_gethostname(buf, sizeof(buf));
		if (result != ISC_R_SUCCESS)
			return (result);
		return (put_txt(lookup, buf));
	}
}

static isc_result_t
do_authors_lookup(dns_sdblookup_t *lookup) {
	isc_result_t result;
	const char **p;
	static const char *authors[] = {
		"Mark Andrews",
		"Curtis Blackburn",
		"James Brister",
		"Ben Cottrell",
		"Michael Graff",
		"Andreas Gustafsson",
		"Bob Halley",
		"Evan Hunt",
		"JINMEI Tatuya",
		"David Lawrence",
		"Scott Mann",
		"Danny Mayer",
		"Damien Neil",
		"Matt Nelson",
		"Jeremy C. Reed",
		"Michael Sawyer",
		"Brian Wellington",
		NULL
	};

	/*
	 * If a version string is specified, disable the authors.bind zone.
	 */
	if (ns_g_server->version_set)
		return (ISC_R_SUCCESS);

	for (p = authors; *p != NULL; p++) {
		result = put_txt(lookup, *p);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	return (ISC_R_SUCCESS);
}

static isc_result_t
do_id_lookup(dns_sdblookup_t *lookup) {

	if (ns_g_server->server_usehostname) {
		char buf[256];
		isc_result_t result = ns_os_gethostname(buf, sizeof(buf));
		if (result != ISC_R_SUCCESS)
			return (result);
		return (put_txt(lookup, buf));
	}

	if (ns_g_server->server_id == NULL)
		return (ISC_R_SUCCESS);
	else
		return (put_txt(lookup, ns_g_server->server_id));
}

static isc_result_t
do_dns64_lookup(dns_sdblookup_t *lookup) {
	UNUSED(lookup);
	return (ISC_R_SUCCESS);
}

static isc_result_t
do_empty_lookup(dns_sdblookup_t *lookup) {

	UNUSED(lookup);
	return (ISC_R_SUCCESS);
}

static isc_result_t
builtin_authority(const char *zone, void *dbdata, dns_sdblookup_t *lookup) {
	isc_result_t result;
	const char *contact = "hostmaster";
	const char *server = "@";
	builtin_t *b = (builtin_t *) dbdata;

	UNUSED(zone);
	UNUSED(dbdata);

	if (b == &empty_builtin) {
		server = ".";
		contact = ".";
	} else {
		if (b->server != NULL)
			server = b->server;
		if (b->contact != NULL)
			contact = b->contact;
	}

	result = dns_sdb_putsoa(lookup, server, contact, 0);
	if (result != ISC_R_SUCCESS)
		return (ISC_R_FAILURE);

	result = dns_sdb_putrr(lookup, "ns", 0, server);
	if (result != ISC_R_SUCCESS)
		return (ISC_R_FAILURE);

	return (ISC_R_SUCCESS);
}

static isc_result_t
builtin_create(const char *zone, int argc, char **argv,
	       void *driverdata, void **dbdata)
{
	REQUIRE(argc >= 1);

	UNUSED(zone);
	UNUSED(driverdata);

	if (strcmp(argv[0], "empty") == 0 || strcmp(argv[0], "dns64") == 0) {
		if (argc != 3)
			return (DNS_R_SYNTAX);
	} else if (argc != 1)
		return (DNS_R_SYNTAX);

	if (strcmp(argv[0], "version") == 0)
		*dbdata = &version_builtin;
	else if (strcmp(argv[0], "hostname") == 0)
		*dbdata = &hostname_builtin;
	else if (strcmp(argv[0], "authors") == 0)
		*dbdata = &authors_builtin;
	else if (strcmp(argv[0], "id") == 0)
		*dbdata = &id_builtin;
	else if (strcmp(argv[0], "empty") == 0 ||
		 strcmp(argv[0], "dns64") == 0) {
		builtin_t *empty;
		char *server;
		char *contact;
		/*
		 * We don't want built-in zones to fail.  Fallback to
		 * the static configuration if memory allocation fails.
		 */
		empty = isc_mem_get(ns_g_mctx, sizeof(*empty));
		server = isc_mem_strdup(ns_g_mctx, argv[1]);
		contact = isc_mem_strdup(ns_g_mctx, argv[2]);
		if (empty == NULL || server == NULL || contact == NULL) {
			if (strcmp(argv[0], "empty") == 0)
				*dbdata = &empty_builtin;
			else
				*dbdata = &dns64_builtin;
			if (server != NULL)
				isc_mem_free(ns_g_mctx, server);
			if (contact != NULL)
				isc_mem_free(ns_g_mctx, contact);
			if (empty != NULL)
				isc_mem_put(ns_g_mctx, empty, sizeof (*empty));
		} else {
			if (strcmp(argv[0], "empty") == 0)
				memcpy(empty, &empty_builtin,
				       sizeof (empty_builtin));
			else
				memcpy(empty, &dns64_builtin,
				       sizeof (empty_builtin));
			empty->server = server;
			empty->contact = contact;
			*dbdata = empty;
		}
	} else
		return (ISC_R_NOTIMPLEMENTED);
	return (ISC_R_SUCCESS);
}

static void
builtin_destroy(const char *zone, void *driverdata, void **dbdata) {
	builtin_t *b = (builtin_t *) *dbdata;

	UNUSED(zone);
	UNUSED(driverdata);

	/*
	 * Don't free the static versions.
	 */
	if (*dbdata == &version_builtin || *dbdata == &hostname_builtin ||
	    *dbdata == &authors_builtin || *dbdata == &id_builtin ||
	    *dbdata == &empty_builtin || *dbdata == &dns64_builtin)
		return;

	isc_mem_free(ns_g_mctx, b->server);
	isc_mem_free(ns_g_mctx, b->contact);
	isc_mem_put(ns_g_mctx, b, sizeof (*b));
}

static dns_sdbmethods_t builtin_methods = {
	builtin_lookup,
	builtin_authority,
	NULL,		/* allnodes */
	builtin_create,
	builtin_destroy,
	NULL
};

static dns_sdbmethods_t dns64_methods = {
	NULL,
	builtin_authority,
	NULL,		/* allnodes */
	builtin_create,
	builtin_destroy,
	dns64_lookup,
};

isc_result_t
ns_builtin_init(void) {
	RUNTIME_CHECK(dns_sdb_register("_builtin", &builtin_methods, NULL,
				       DNS_SDBFLAG_RELATIVEOWNER |
				       DNS_SDBFLAG_RELATIVERDATA,
				       ns_g_mctx, &builtin_impl)
		      == ISC_R_SUCCESS);
	RUNTIME_CHECK(dns_sdb_register("_dns64", &dns64_methods, NULL,
				       DNS_SDBFLAG_RELATIVEOWNER |
				       DNS_SDBFLAG_RELATIVERDATA |
				       DNS_SDBFLAG_DNS64,
				       ns_g_mctx, &dns64_impl)
		      == ISC_R_SUCCESS);
	return (ISC_R_SUCCESS);
}

void
ns_builtin_deinit(void) {
	dns_sdb_unregister(&builtin_impl);
	dns_sdb_unregister(&dns64_impl);
}
