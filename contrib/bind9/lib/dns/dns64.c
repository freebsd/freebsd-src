/*
 * Copyright (C) 2010-2012  Internet Systems Consortium, Inc. ("ISC")
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

#include <config.h>

#include <isc/list.h>
#include <isc/mem.h>
#include <isc/netaddr.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/acl.h>
#include <dns/dns64.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/result.h>

struct dns_dns64 {
	unsigned char		bits[16];	/*
						 * Prefix + suffix bits.
						 */
	dns_acl_t *		clients;	/*
						 * Which clients get mapped
						 * addresses.
						 */
	dns_acl_t *		mapped;		/*
						 * IPv4 addresses to be mapped.
						 */
	dns_acl_t *		excluded;	/*
						 * IPv6 addresses that are
						 * treated as not existing.
						 */
	unsigned int		prefixlen;	/*
						 * Start of mapped address.
						 */
	unsigned int		flags;
	isc_mem_t *		mctx;
	ISC_LINK(dns_dns64_t)	link;
};

isc_result_t
dns_dns64_create(isc_mem_t *mctx, isc_netaddr_t *prefix,
		 unsigned int prefixlen, isc_netaddr_t *suffix,
		 dns_acl_t *clients, dns_acl_t *mapped, dns_acl_t *excluded,
		 unsigned int flags, dns_dns64_t **dns64)
{
	dns_dns64_t *new;
	unsigned int nbytes = 16;

	REQUIRE(prefix != NULL && prefix->family == AF_INET6);
	/* Legal prefix lengths from draft-ietf-behave-address-format-04. */
	REQUIRE(prefixlen == 32 || prefixlen == 40 || prefixlen == 48 ||
		prefixlen == 56 || prefixlen == 64 || prefixlen == 96);
	REQUIRE(isc_netaddr_prefixok(prefix, prefixlen) == ISC_R_SUCCESS);
	REQUIRE(dns64 != NULL && *dns64 == NULL);

	if (suffix != NULL) {
		static const unsigned char zeros[16];
		REQUIRE(prefix->family == AF_INET6);
		nbytes = prefixlen / 8 + 4;
		/* Bits 64-71 are zeros. draft-ietf-behave-address-format-04 */
		if (prefixlen >= 32 && prefixlen <= 64)
			nbytes++;
		REQUIRE(memcmp(suffix->type.in6.s6_addr, zeros, nbytes) == 0);
	}

	new = isc_mem_get(mctx, sizeof(dns_dns64_t));
	if (new == NULL)
		return (ISC_R_NOMEMORY);
	memset(new->bits, 0, sizeof(new->bits));
	memcpy(new->bits, prefix->type.in6.s6_addr, prefixlen / 8);
	if (suffix != NULL)
		memcpy(new->bits + nbytes, suffix->type.in6.s6_addr + nbytes,
		       16 - nbytes);
	new->clients = NULL;
	if (clients != NULL)
		dns_acl_attach(clients, &new->clients);
	new->mapped = NULL;
	if (mapped != NULL)
		dns_acl_attach(mapped, &new->mapped);
	new->excluded = NULL;
	if (excluded != NULL)
		dns_acl_attach(excluded, &new->excluded);
	new->prefixlen = prefixlen;
	new->flags = flags;
	ISC_LINK_INIT(new, link);
	new->mctx = NULL;
	isc_mem_attach(mctx, &new->mctx);
	*dns64 = new;
	return (ISC_R_SUCCESS);
}

void
dns_dns64_destroy(dns_dns64_t **dns64p) {
	dns_dns64_t *dns64;

	REQUIRE(dns64p != NULL && *dns64p != NULL);

	dns64 = *dns64p;
	*dns64p = NULL;

	REQUIRE(!ISC_LINK_LINKED(dns64, link));

	if (dns64->clients != NULL)
		dns_acl_detach(&dns64->clients);
	if (dns64->mapped != NULL)
		dns_acl_detach(&dns64->mapped);
	if (dns64->excluded != NULL)
		dns_acl_detach(&dns64->excluded);
	isc_mem_putanddetach(&dns64->mctx, dns64, sizeof(*dns64));
}

isc_result_t
dns_dns64_aaaafroma(const dns_dns64_t *dns64, const isc_netaddr_t *reqaddr,
		    const dns_name_t *reqsigner, const dns_aclenv_t *env,
		    unsigned int flags, unsigned char *a, unsigned char *aaaa)
{
	unsigned int nbytes, i;
	isc_result_t result;
	int match;

	if ((dns64->flags & DNS_DNS64_RECURSIVE_ONLY) != 0 &&
	    (flags & DNS_DNS64_RECURSIVE) == 0)
		return (DNS_R_DISALLOWED);

	if ((dns64->flags & DNS_DNS64_BREAK_DNSSEC) == 0 &&
	    (flags & DNS_DNS64_DNSSEC) != 0)
		return (DNS_R_DISALLOWED);

	if (dns64->clients != NULL) {
		result = dns_acl_match(reqaddr, reqsigner, dns64->clients, env,
				       &match, NULL);
		if (result != ISC_R_SUCCESS)
			return (result);
		if (match <= 0)
			return (DNS_R_DISALLOWED);
	}

	if (dns64->mapped != NULL) {
		struct in_addr ina;
		isc_netaddr_t netaddr;

		memcpy(&ina.s_addr, a, 4);
		isc_netaddr_fromin(&netaddr, &ina);
		result = dns_acl_match(&netaddr, NULL, dns64->mapped, env,
				       &match, NULL);
		if (result != ISC_R_SUCCESS)
			return (result);
		if (match <= 0)
			return (DNS_R_DISALLOWED);
	}

	nbytes = dns64->prefixlen / 8;
	INSIST(nbytes <= 12);
	/* Copy prefix. */
	memcpy(aaaa, dns64->bits, nbytes);
	/* Bits 64-71 are zeros. draft-ietf-behave-address-format-04 */
	if (nbytes == 8)
		aaaa[nbytes++] = 0;
	/* Copy mapped address. */
	for (i = 0; i < 4U; i++) {
		aaaa[nbytes++] = a[i];
		/* Bits 64-71 are zeros. draft-ietf-behave-address-format-04 */
		if (nbytes == 8)
			aaaa[nbytes++] = 0;
	}
	/* Copy suffix. */
	memcpy(aaaa + nbytes, dns64->bits + nbytes, 16 - nbytes);
	return (ISC_R_SUCCESS);
}

dns_dns64_t *
dns_dns64_next(dns_dns64_t *dns64) {
	dns64 = ISC_LIST_NEXT(dns64, link);
	return (dns64);
}

void
dns_dns64_append(dns_dns64list_t *list, dns_dns64_t *dns64) {
	ISC_LIST_APPEND(*list, dns64, link);
}

void
dns_dns64_unlink(dns_dns64list_t *list, dns_dns64_t *dns64) {
	ISC_LIST_UNLINK(*list, dns64, link);
}

isc_boolean_t
dns_dns64_aaaaok(const dns_dns64_t *dns64, const isc_netaddr_t *reqaddr,
		 const dns_name_t *reqsigner, const dns_aclenv_t *env,
		 unsigned int flags, dns_rdataset_t *rdataset,
		 isc_boolean_t *aaaaok, size_t aaaaoklen)
{
	struct in6_addr in6;
	isc_netaddr_t netaddr;
	isc_result_t result;
	int match;
	isc_boolean_t answer = ISC_FALSE;
	isc_boolean_t found = ISC_FALSE;
	unsigned int i, ok;

	REQUIRE(rdataset != NULL);
	REQUIRE(rdataset->type == dns_rdatatype_aaaa);
	REQUIRE(rdataset->rdclass == dns_rdataclass_in);
	if (aaaaok != NULL)
		REQUIRE(aaaaoklen == dns_rdataset_count(rdataset));

	for (;dns64 != NULL; dns64 = ISC_LIST_NEXT(dns64, link)) {
		if ((dns64->flags & DNS_DNS64_RECURSIVE_ONLY) != 0 &&
		    (flags & DNS_DNS64_RECURSIVE) == 0)
			continue;

		if ((dns64->flags & DNS_DNS64_BREAK_DNSSEC) == 0 &&
		    (flags & DNS_DNS64_DNSSEC) != 0)
			continue;
		/*
		 * Work out if this dns64 structure applies to this client.
		 */
		if (dns64->clients != NULL) {
			result = dns_acl_match(reqaddr, reqsigner,
					       dns64->clients, env,
					       &match, NULL);
			if (result != ISC_R_SUCCESS)
				continue;
			if (match <= 0)
				continue;
		}

		if (!found && aaaaok != NULL) {
			for (i = 0; i < aaaaoklen; i++)
				aaaaok[i] = ISC_FALSE;
		}
		found = ISC_TRUE;

		/*
		 * If we are not excluding any addresses then any AAAA
		 * will do.
		 */
		if (dns64->excluded == NULL) {
			answer = ISC_TRUE;
			if (aaaaok == NULL)
				goto done;
			for (i = 0; i < aaaaoklen; i++)
				aaaaok[i] = ISC_TRUE;
			goto done;
		}

		i = 0; ok = 0;
		for (result = dns_rdataset_first(rdataset);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(rdataset)) {
			dns_rdata_t rdata = DNS_RDATA_INIT;
			if (aaaaok == NULL || !aaaaok[i]) {

				dns_rdataset_current(rdataset, &rdata);
				memcpy(&in6.s6_addr, rdata.data, 16);
				isc_netaddr_fromin6(&netaddr, &in6);

				result = dns_acl_match(&netaddr, NULL,
						       dns64->excluded,
						       env, &match, NULL);
				if (result == ISC_R_SUCCESS && match <= 0) {
					answer = ISC_TRUE;
					if (aaaaok == NULL)
						goto done;
					aaaaok[i] = ISC_TRUE;
					ok++;
				}
			} else
				ok++;
			i++;
		}
		/*
		 * Are all addresses ok?
		 */
		if (aaaaok != NULL && ok == aaaaoklen)
			goto done;
	}

 done:
	if (!found && aaaaok != NULL) {
		for (i = 0; i < aaaaoklen; i++)
			aaaaok[i] = ISC_TRUE;
	}
	return (found ? answer : ISC_TRUE);
}
