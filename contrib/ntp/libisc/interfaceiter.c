/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: interfaceiter.c,v 1.27 2002/07/02 05:51:43 marka Exp $ */

#include <config.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>		/* Required for ifiter_ioctl.c. */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <isc/interfaceiter.h>
#include <isc/magic.h>
#include <isc/mem.h>
#include <isc/msgs.h>
#include <isc/net.h>
#include <isc/result.h>
#include <isc/strerror.h>
#include <isc/string.h>
#include <isc/types.h>
#include <isc/util.h>

/* Must follow <isc/net.h>. */
#ifdef HAVE_NET_IF6_H
#include <net/if6.h>
#endif
#include <net/if.h>

/* Common utility functions */

/*
 * Extract the network address part from a "struct sockaddr".
 *
 * The address family is given explicity
 * instead of using src->sa_family, because the latter does not work
 * for copying a network mask obtained by SIOCGIFNETMASK (it does
 * not have a valid address family).
 */

static void
get_addr(unsigned int family, isc_netaddr_t *dst, struct sockaddr *src) {
	dst->family = family;
	switch (family) {
	case AF_INET:
		memcpy(&dst->type.in,
		       &((struct sockaddr_in *) src)->sin_addr,
		       sizeof(struct in_addr));
		break;
	case	AF_INET6:
		memcpy(&dst->type.in6,
		       &((struct sockaddr_in6 *) src)->sin6_addr,
		       sizeof(struct in6_addr));
		break;
	default:
		INSIST(0);
		break;
	}
}

/*
 * Include system-dependent code.
 */

#if HAVE_IFLIST_SYSCTL
#include "ifiter_sysctl.c"
#else
#include "ifiter_ioctl.c"
#endif

/*
 * The remaining code is common to the sysctl and ioctl case.
 */

isc_result_t
isc_interfaceiter_current(isc_interfaceiter_t *iter,
			  isc_interface_t *ifdata)
{
	REQUIRE(iter->result == ISC_R_SUCCESS);
	memcpy(ifdata, &iter->current, sizeof(*ifdata));
	return (ISC_R_SUCCESS);
}

isc_result_t
isc_interfaceiter_first(isc_interfaceiter_t *iter) {
	isc_result_t result;

	REQUIRE(VALID_IFITER(iter));

	iter->pos = 0;
#ifdef HAVE_TRUCLUSTER
	iter->clua_context = 0;
#endif
	for (;;) {
		result = internal_current(iter);
		if (result != ISC_R_IGNORE)
			break;
		result = internal_next(iter);
		if (result != ISC_R_SUCCESS)
			break;
	}
	iter->result = result;
	return (result);
}

isc_result_t
isc_interfaceiter_next(isc_interfaceiter_t *iter) {
	isc_result_t result;

	REQUIRE(VALID_IFITER(iter));
	REQUIRE(iter->result == ISC_R_SUCCESS);

	for (;;) {
		result = internal_next(iter);
		if (result != ISC_R_SUCCESS)
			break;
		result = internal_current(iter);
		if (result != ISC_R_IGNORE)
			break;
	}
	iter->result = result;
	return (result);
}

void
isc_interfaceiter_destroy(isc_interfaceiter_t **iterp)
{
	isc_interfaceiter_t *iter;
	REQUIRE(iterp != NULL);
	iter = *iterp;
	REQUIRE(VALID_IFITER(iter));

	internal_destroy(iter);
	isc_mem_put(iter->mctx, iter->buf, iter->bufsize);

	iter->magic = 0;
	isc_mem_put(iter->mctx, iter, sizeof(*iter));
	*iterp = NULL;
}
