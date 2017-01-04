/*
 *
 *        Copyright 1989, 1991, 1992 by Carnegie Mellon University
 *
 *  		  Derivative Work - 1996, 1998-2000
 * Copyright 1996, 1998-2000 The Regents of the University of California
 *
 *  			 All Rights Reserved
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of CMU and The Regents of
 * the University of California not be used in advertising or publicity
 * pertaining to distribution of the software without specific written
 * permission.
 *
 * CMU AND THE REGENTS OF THE UNIVERSITY OF CALIFORNIA DISCLAIM ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL CMU OR
 * THE REGENTS OF THE UNIVERSITY OF CALIFORNIA BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM THE LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

/*
 * From <net-snmp>/agent/mibgroup/mibII/ipv6.c, and maybe originally from the
 * Kame project
 */

/*
 * TODO (ngie): replace with network_get_interfaces(..) from
 * snmp_hostres/hostres_network_tbl.c so we can ditch this completely.
 */

/* $FreeBSD$ */

#include <strings.h>

#include "util.h"

int
if_getifmibdata(int idx, struct ifmibdata *result)
{
	int mib[] = {
		CTL_NET,
		PF_LINK,
		NETLINK_GENERIC,
		IFMIB_IFDATA,
		0,
		IFDATA_GENERAL
	};
	size_t len;
	struct ifmibdata tmp;

	mib[4] = idx;
	len = sizeof(struct ifmibdata);
	if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), &tmp, &len, 0, 0) < 0)
		return -1;
	memcpy(result, &tmp, sizeof(tmp));
	return 0;
}
