/*
 * Copyright (C) 2009, 2011-2013  Internet Systems Consortium, Inc. ("ISC")
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

/*! \file */

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 *    getnameinfo() returns the hostname for the struct sockaddr sa which is
 *    salen bytes long. The hostname is of length hostlen and is returned via
 *    *host. The maximum length of the hostname is 1025 bytes: #NI_MAXHOST.
 *
 *    The name of the service associated with the port number in sa is
 *    returned in *serv. It is servlen bytes long. The maximum length of the
 *    service name is #NI_MAXSERV - 32 bytes.
 *
 *    The flags argument sets the following bits:
 *
 * \li   #NI_NOFQDN:
 *           A fully qualified domain name is not required for local hosts.
 *           The local part of the fully qualified domain name is returned
 *           instead.
 *
 * \li   #NI_NUMERICHOST
 *           Return the address in numeric form, as if calling inet_ntop(),
 *           instead of a host name.
 *
 * \li   #NI_NAMEREQD
 *           A name is required. If the hostname cannot be found in the DNS
 *           and this flag is set, a non-zero error code is returned. If the
 *           hostname is not found and the flag is not set, the address is
 *           returned in numeric form.
 *
 * \li   #NI_NUMERICSERV
 *           The service name is returned as a digit string representing the
 *           port number.
 *
 * \li   #NI_DGRAM
 *           Specifies that the service being looked up is a datagram
 *           service, and causes getservbyport() to be called with a second
 *           argument of "udp" instead of its default of "tcp". This is
 *           required for the few ports (512-514) that have different
 *           services for UDP and TCP.
 *
 * \section getnameinfo_return Return Values
 *
 *    getnameinfo() returns 0 on success or a non-zero error code if
 *    an error occurs.
 *
 * \section getname_see See Also
 *
 *    RFC3493, getservbyport(),
 *    getnamebyaddr(). inet_ntop().
 */

#include <config.h>

#include <stdio.h>
#include <string.h>

#include <isc/netaddr.h>
#include <isc/print.h>
#include <isc/sockaddr.h>
#include <isc/util.h>

#include <dns/byaddr.h>
#include <dns/client.h>
#include <dns/fixedname.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdatastruct.h>
#include <dns/result.h>

#include <irs/context.h>
#include <irs/netdb.h>

#define SUCCESS 0

/*% afd structure definition */
static struct afd {
	int a_af;
	size_t a_addrlen;
	size_t a_socklen;
} afdl [] = {
	/*!
	 * First entry is linked last...
	 */
	{ AF_INET, sizeof(struct in_addr), sizeof(struct sockaddr_in) },
	{ AF_INET6, sizeof(struct in6_addr), sizeof(struct sockaddr_in6) },
	{0, 0, 0},
};

/*!
 * The test against 0 is there to keep the Solaris compiler
 * from complaining about "end-of-loop code not reached".
 */
#define ERR(code) \
	do { result = (code);			\
		if (result != 0) goto cleanup;	\
	} while (0)

int
getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host,
	    IRS_GETNAMEINFO_BUFLEN_T hostlen, char *serv,
	    IRS_GETNAMEINFO_BUFLEN_T servlen, IRS_GETNAMEINFO_FLAGS_T flags)
{
	struct afd *afd = NULL;
	struct servent *sp;
	unsigned short port = 0;
#ifdef IRS_PLATFORM_HAVESALEN
	size_t len;
#endif
	int family, i;
	const void *addr = NULL;
	char *p;
#if 0
	unsigned long v4a;
	unsigned char pfx;
#endif
	char numserv[sizeof("65000")];
	char numaddr[sizeof("abcd:abcd:abcd:abcd:abcd:abcd:255.255.255.255")
		    + 1 + sizeof("4294967295")];
	const char *proto;
	int result = SUCCESS;

	if (sa == NULL)
		ERR(EAI_FAIL);

#ifdef IRS_PLATFORM_HAVESALEN
	len = sa->sa_len;
	if (len != salen)
		ERR(EAI_FAIL);
#endif

	family = sa->sa_family;
	for (i = 0; afdl[i].a_af; i++)
		if (afdl[i].a_af == family) {
			afd = &afdl[i];
			goto found;
		}
	ERR(EAI_FAMILY);

 found:
	if (salen != afd->a_socklen)
		ERR(EAI_FAIL);

	switch (family) {
	case AF_INET:
		port = ((const struct sockaddr_in *)sa)->sin_port;
		addr = &((const struct sockaddr_in *)sa)->sin_addr.s_addr;
		break;

	case AF_INET6:
		port = ((const struct sockaddr_in6 *)sa)->sin6_port;
		addr = ((const struct sockaddr_in6 *)sa)->sin6_addr.s6_addr;
		break;

	default:
		INSIST(0);
	}
	proto = (flags & NI_DGRAM) ? "udp" : "tcp";

	if (serv == NULL || servlen == 0U) {
		/*
		 * Caller does not want service.
		 */
	} else if ((flags & NI_NUMERICSERV) != 0 ||
		   (sp = getservbyport(port, proto)) == NULL) {
		snprintf(numserv, sizeof(numserv), "%d", ntohs(port));
		if ((strlen(numserv) + 1) > servlen)
			ERR(EAI_OVERFLOW);
		strcpy(serv, numserv);
	} else {
		if ((strlen(sp->s_name) + 1) > servlen)
			ERR(EAI_OVERFLOW);
		strcpy(serv, sp->s_name);
	}

#if 0
	switch (sa->sa_family) {
	case AF_INET:
		v4a = ((struct sockaddr_in *)sa)->sin_addr.s_addr;
		if (IN_MULTICAST(v4a) || IN_EXPERIMENTAL(v4a))
			flags |= NI_NUMERICHOST;
		v4a >>= IN_CLASSA_NSHIFT;
		if (v4a == 0 || v4a == IN_LOOPBACKNET)
			flags |= NI_NUMERICHOST;
		break;

	case AF_INET6:
		pfx = ((struct sockaddr_in6 *)sa)->sin6_addr.s6_addr[0];
		if (pfx == 0 || pfx == 0xfe || pfx == 0xff)
			flags |= NI_NUMERICHOST;
		break;
	}
#endif

	if (host == NULL || hostlen == 0U) {
		/*
		 * do nothing in this case.
		 * in case you are wondering if "&&" is more correct than
		 * "||" here: RFC3493 says that host == NULL or hostlen == 0
		 * means that the caller does not want the result.
		 */
	} else if ((flags & NI_NUMERICHOST) != 0) {
		if (inet_ntop(afd->a_af, addr, numaddr, sizeof(numaddr))
		    == NULL)
			ERR(EAI_SYSTEM);
#if defined(IRS_HAVE_SIN6_SCOPE_ID)
		if (afd->a_af == AF_INET6 &&
		    ((const struct sockaddr_in6 *)sa)->sin6_scope_id) {
			char *p = numaddr + strlen(numaddr);
			const char *stringscope = NULL;
#ifdef VENDOR_SPECIFIC
			/*
			 * Vendors may want to add support for
			 * non-numeric scope identifier.
			 */
			stringscope = foo;
#endif
			if (stringscope == NULL) {
				snprintf(p, sizeof(numaddr) - (p - numaddr),
				    "%%%u",
				    ((const struct sockaddr_in6 *)sa)->sin6_scope_id);
			} else {
				snprintf(p, sizeof(numaddr) - (p - numaddr),
				    "%%%s", stringscope);
			}
		}
#endif
		if (strlen(numaddr) + 1 > hostlen)
			ERR(EAI_OVERFLOW);
		strcpy(host, numaddr);
	} else {
		isc_netaddr_t netaddr;
		dns_fixedname_t ptrfname;
		dns_name_t *ptrname;
		irs_context_t *irsctx = NULL;
		dns_client_t *client;
		isc_boolean_t found = ISC_FALSE;
		dns_namelist_t answerlist;
		dns_rdataset_t *rdataset;
		isc_region_t hostregion;
		char hoststr[1024]; /* is this enough? */
		isc_result_t iresult;

		/* Get IRS context and the associated DNS client object */
		iresult = irs_context_get(&irsctx);
		if (iresult != ISC_R_SUCCESS)
			ERR(EAI_FAIL);
		client = irs_context_getdnsclient(irsctx);

		/* Make query name */
		isc_netaddr_fromsockaddr(&netaddr, (const isc_sockaddr_t *)sa);
		dns_fixedname_init(&ptrfname);
		ptrname = dns_fixedname_name(&ptrfname);
		iresult = dns_byaddr_createptrname2(&netaddr, 0, ptrname);
		if (iresult != ISC_R_SUCCESS)
			ERR(EAI_FAIL);

		/* Get the PTR RRset */
		ISC_LIST_INIT(answerlist);
		iresult = dns_client_resolve(client, ptrname,
					     dns_rdataclass_in,
					     dns_rdatatype_ptr,
					     DNS_CLIENTRESOPT_ALLOWRUN,
					     &answerlist);
		switch (iresult) {
		case ISC_R_SUCCESS:
		/*
		 * a 'non-existent' error is not necessarily fatal for
		 * getnameinfo().
		 */
		case DNS_R_NCACHENXDOMAIN:
		case DNS_R_NCACHENXRRSET:
			break;
		case DNS_R_SIGINVALID:
		case DNS_R_SIGEXPIRED:
		case DNS_R_SIGFUTURE:
		case DNS_R_KEYUNAUTHORIZED:
		case DNS_R_MUSTBESECURE:
		case DNS_R_COVERINGNSEC:
		case DNS_R_NOTAUTHORITATIVE:
		case DNS_R_NOVALIDKEY:
		case DNS_R_NOVALIDDS:
		case DNS_R_NOVALIDSIG:
			ERR(EAI_INSECUREDATA);
			break;
		default:
			ERR(EAI_FAIL);
		}

		/* Parse the answer for the hostname */
		for (ptrname = ISC_LIST_HEAD(answerlist); ptrname != NULL;
		     ptrname = ISC_LIST_NEXT(ptrname, link)) {
			for (rdataset = ISC_LIST_HEAD(ptrname->list);
			     rdataset != NULL;
			     rdataset = ISC_LIST_NEXT(rdataset, link)) {
				if (!dns_rdataset_isassociated(rdataset))
					continue;
				if (rdataset->type != dns_rdatatype_ptr)
					continue;

				for (iresult = dns_rdataset_first(rdataset);
				     iresult == ISC_R_SUCCESS;
				     iresult = dns_rdataset_next(rdataset)) {
					dns_rdata_t rdata;
					dns_rdata_ptr_t rdata_ptr;
					isc_buffer_t b;

					dns_rdata_init(&rdata);
					dns_rdataset_current(rdataset, &rdata);
					dns_rdata_tostruct(&rdata, &rdata_ptr,
							   NULL);

					isc_buffer_init(&b, hoststr,
							sizeof(hoststr));
					iresult =
						dns_name_totext(&rdata_ptr.ptr,
								ISC_TRUE, &b);
					dns_rdata_freestruct(&rdata_ptr);
					if (iresult == ISC_R_SUCCESS) {
						/*
						 * We ignore the rest of the
						 * answer.  After all,
						 * getnameinfo() can return
						 * at most one hostname.
						 */
						found = ISC_TRUE;
						isc_buffer_usedregion(
							&b, &hostregion);
						goto ptrfound;
					}

				}
			}
		}
	ptrfound:
		dns_client_freeresanswer(client, &answerlist);
		if (found) {
			if ((flags & NI_NOFQDN) != 0) {
				p = strchr(hoststr, '.');
				if (p)
					*p = '\0';
			}
			if (hostregion.length + 1 > hostlen)
				ERR(EAI_OVERFLOW);
			snprintf(host, hostlen, "%.*s",
				 (int)hostregion.length,
				 (char *)hostregion.base);
		} else {
			if ((flags & NI_NAMEREQD) != 0)
				ERR(EAI_NONAME);
			if (inet_ntop(afd->a_af, addr, numaddr,
				      sizeof(numaddr)) == NULL)
				ERR(EAI_SYSTEM);
			if ((strlen(numaddr) + 1) > hostlen)
				ERR(EAI_OVERFLOW);
			strcpy(host, numaddr);
		}
	}
	result = SUCCESS;

 cleanup:
	return (result);
}
