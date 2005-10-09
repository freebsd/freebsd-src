/*
 * Copyright (c) 2000
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Id: findsaddr-linux.c,v 1.1 2000/11/23 20:17:12 leres Exp $ (LBL)";
#endif

/* XXX linux is different (as usual) */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#include <sys/time.h>				/* concession to AIX */

#if __STDC__
struct mbuf;
struct rtentry;
#endif

#include <net/if.h>
#include <netinet/in.h>

#include <arpa/inet.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gnuc.h"
#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#include "findsaddr.h"
#include "ifaddrlist.h"
#include "traceroute.h"

static const char route[] = "/proc/net/route";

/*
 * Return the source address for the given destination address
 */
const char *
findsaddr(register const struct sockaddr_in *to,
    register struct sockaddr_in *from)
{
	register int i, n;
	register FILE *f;
	register u_int32_t mask;
	u_int32_t dest, tmask;
	struct ifaddrlist *al;
	char buf[256], tdevice[256], device[256];
	static char errbuf[132];

	if ((f = fopen(route, "r")) == NULL) {
		sprintf(errbuf, "open %s: %.128s", route, strerror(errno));
		return (errbuf);
	}

	/* Find the appropriate interface */
	n = 0;
	mask = 0;
	device[0] = '\0';
	while (fgets(buf, sizeof(buf), f) != NULL) {
		++n;
		if (n == 1 && strncmp(buf, "Iface", 5) == 0)
			continue;
		if ((i = sscanf(buf, "%s %x %*s %*s %*s %*s %*s %x",
		    tdevice, &dest, &tmask)) != 3)
			return ("junk in buffer");
		if ((to->sin_addr.s_addr & tmask) == dest &&
		    (tmask > mask || mask == 0)) {
			mask = tmask;
			strcpy(device, tdevice);
		}
	}
	fclose(f);

	if (device[0] == '\0')
		return ("Can't find interface");

        /* Get the interface address list */
	if ((n = ifaddrlist(&al, errbuf)) < 0)
		return (errbuf);

	if (n == 0)
		return ("Can't find any network interfaces");

	/* Find our appropriate source address */
	for (i = n; i > 0; --i, ++al)
		if (strcmp(device, al->device) == 0)
			break;
	if (i <= 0) {
		sprintf(errbuf, "Can't find interface \"%.32s\"", device);
		return (errbuf);
	}

	setsin(from, al->addr);
	return (NULL);
}
