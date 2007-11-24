/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
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

/* $Id: ifiter_getifaddrs.c,v 1.2.68.3 2004/03/06 08:14:59 marka Exp $ */

/*
 * Obtain the list of network interfaces using the getifaddrs(3) library.
 */

#include <ifaddrs.h>

#define IFITER_MAGIC		ISC_MAGIC('I', 'F', 'I', 'G')
#define VALID_IFITER(t)		ISC_MAGIC_VALID(t, IFITER_MAGIC)

struct isc_interfaceiter {
	unsigned int		magic;		/* Magic number. */
	isc_mem_t		*mctx;
	void			*buf;		/* (unused) */
	unsigned int		bufsize;	/* (always 0) */
	struct ifaddrs		*ifaddrs;	/* List of ifaddrs */
	struct ifaddrs		*pos;		/* Ptr to current ifaddr */
	isc_interface_t		current;	/* Current interface data. */
	isc_result_t		result;		/* Last result code. */
};

isc_result_t
isc_interfaceiter_create(isc_mem_t *mctx, isc_interfaceiter_t **iterp) {
	isc_interfaceiter_t *iter;
	isc_result_t result;
	char strbuf[ISC_STRERRORSIZE];

	REQUIRE(mctx != NULL);
	REQUIRE(iterp != NULL);
	REQUIRE(*iterp == NULL);

	iter = isc_mem_get(mctx, sizeof(*iter));
	if (iter == NULL)
		return (ISC_R_NOMEMORY);

	iter->mctx = mctx;
	iter->buf = NULL;
	iter->bufsize = 0;
	iter->ifaddrs = NULL;

	if (getifaddrs(&iter->ifaddrs) < 0) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 isc_msgcat_get(isc_msgcat,
						ISC_MSGSET_IFITERGETIFADDRS,
						ISC_MSG_GETIFADDRS,
						"getting interface "
						"addresses: getifaddrs: %s"),
				 strbuf);
		result = ISC_R_UNEXPECTED;
		goto failure;
	}

	/*
	 * A newly created iterator has an undefined position
	 * until isc_interfaceiter_first() is called.
	 */
	iter->pos = NULL;
	iter->result = ISC_R_FAILURE;

	iter->magic = IFITER_MAGIC;
	*iterp = iter;
	return (ISC_R_SUCCESS);

 failure:
	if (iter->ifaddrs != NULL) /* just in case */
		freeifaddrs(iter->ifaddrs);
	isc_mem_put(mctx, iter, sizeof(*iter));
	return (result);
}

/*
 * Get information about the current interface to iter->current.
 * If successful, return ISC_R_SUCCESS.
 * If the interface has an unsupported address family,
 * return ISC_R_IGNORE.
 */

static isc_result_t
internal_current(isc_interfaceiter_t *iter) {
	struct ifaddrs *ifa;
	int family;
	unsigned int namelen;

	REQUIRE(VALID_IFITER(iter));

	ifa = iter->pos;

	INSIST(ifa != NULL);
	INSIST(ifa->ifa_name != NULL);
	INSIST(ifa->ifa_addr != NULL);

	family = ifa->ifa_addr->sa_family;
	if (family != AF_INET && family != AF_INET6)
		return (ISC_R_IGNORE);

	memset(&iter->current, 0, sizeof(iter->current));

	namelen = strlen(ifa->ifa_name);
	if (namelen > sizeof(iter->current.name) - 1)
		namelen = sizeof(iter->current.name) - 1;

	memset(iter->current.name, 0, sizeof(iter->current.name));
	memcpy(iter->current.name, ifa->ifa_name, namelen);

	iter->current.flags = 0;

	if ((ifa->ifa_flags & IFF_UP) != 0)
		iter->current.flags |= INTERFACE_F_UP;

	if ((ifa->ifa_flags & IFF_POINTOPOINT) != 0)
		iter->current.flags |= INTERFACE_F_POINTTOPOINT;

	if ((ifa->ifa_flags & IFF_LOOPBACK) != 0)
		iter->current.flags |= INTERFACE_F_LOOPBACK;

	iter->current.af = family;

	get_addr(family, &iter->current.address, ifa->ifa_addr, ifa->ifa_name);

	if (ifa->ifa_netmask != NULL)
		get_addr(family, &iter->current.netmask, ifa->ifa_netmask,
			 ifa->ifa_name);

	if (ifa->ifa_dstaddr != NULL &&
	    (iter->current.flags & IFF_POINTOPOINT) != 0)
		get_addr(family, &iter->current.dstaddress, ifa->ifa_dstaddr,
			 ifa->ifa_name);

	return (ISC_R_SUCCESS);
}

/*
 * Step the iterator to the next interface.  Unlike
 * isc_interfaceiter_next(), this may leave the iterator
 * positioned on an interface that will ultimately
 * be ignored.  Return ISC_R_NOMORE if there are no more
 * interfaces, otherwise ISC_R_SUCCESS.
 */
static isc_result_t
internal_next(isc_interfaceiter_t *iter) {
	iter->pos = iter->pos->ifa_next;

	if (iter->pos == NULL)
		return (ISC_R_NOMORE);

	return (ISC_R_SUCCESS);
}

static void
internal_destroy(isc_interfaceiter_t *iter) {
	if (iter->ifaddrs)
		freeifaddrs(iter->ifaddrs);
	iter->ifaddrs = NULL;
}

static
void internal_first(isc_interfaceiter_t *iter) {
	iter->pos = iter->ifaddrs;
}
