/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if defined(LIBC_SCCS) && !defined(lint)
/*static char *sccsid = "from: @(#)get_myaddress.c 1.4 87/08/11 Copyr 1984 Sun Micro";*/
/*static char *sccsid = "from: @(#)get_myaddress.c	2.1 88/07/29 4.0 RPCSRC";*/
static char *rcsid = "$FreeBSD: src/lib/libc/rpc/get_myaddress.c,v 1.18 2000/03/03 13:04:58 shin Exp $";
#endif

/*
 * get_myaddress.c
 *
 * Get client's IP address via ioctl.  This avoids using the yellowpages.
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/pmap_prot.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * don't use gethostbyname, which would invoke yellow pages
 *
 * Avoid loopback interfaces.  We return information from a loopback
 * interface only if there are no other possible interfaces.
 */
int
get_myaddress(addr)
	struct sockaddr_in *addr;
{
	int s;
	char buf[BUFSIZ];
	struct ifconf ifc;
	struct ifreq ifreq, *ifr, *end;
	int loopback = 0, gotit = 0;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		return(-1);
	}
	ifc.ifc_len = sizeof (buf);
	ifc.ifc_buf = buf;
	if (ioctl(s, SIOCGIFCONF, (char *)&ifc) < 0) {
		_close(s);
		return(-1);
	}
again:
	ifr = ifc.ifc_req;
	end = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);

	while (ifr < end) {
		memcpy(&ifreq, ifr, sizeof(ifreq));
		if (ioctl(s, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
			_close(s);
			return(-1);
		}
		if (((ifreq.ifr_flags & IFF_UP) &&
		    ifr->ifr_addr.sa_family == AF_INET &&
			!(ifreq.ifr_flags & IFF_LOOPBACK)) ||
		    (loopback == 1 && (ifreq.ifr_flags & IFF_LOOPBACK)
			&& (ifr->ifr_addr.sa_family == AF_INET)
			&& (ifreq.ifr_flags &  IFF_UP))) {
			*addr = *((struct sockaddr_in *)&ifr->ifr_addr);
			addr->sin_port = htons(PMAPPORT);
			gotit = 1;
			break;
		}
		if (ifr->ifr_addr.sa_len)
			ifr = (struct ifreq *) ((caddr_t) ifr +
			      ifr->ifr_addr.sa_len -
			      sizeof(struct sockaddr));
		ifr++;
	}
	if (gotit == 0 && loopback == 0) {
		loopback = 1;
		goto again;
	}
	(void)_close(s);
	return (gotit ? 0 : -1);
}
