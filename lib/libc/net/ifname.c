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
 *
 * $FreeBSD: src/lib/libc/net/ifname.c,v 1.1 1999/12/16 18:32:01 shin Exp $
 */
/*
 * TODO:
 * - prototype defs into arpa/inet.h, not net/if.h (bsd-api-new-02)
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

unsigned int
if_nametoindex(ifname)
	const char *ifname;
{
	struct if_nameindex *iff = if_nameindex(), *ifx;
	int ret;

	if (iff == NULL) return 0;
	ifx = iff;
	while (ifx->if_name != NULL) {
		if (strcmp(ifx->if_name, ifname) == 0) {
			ret = ifx->if_index;
			if_freenameindex(iff);
			return ret;
		}
		ifx++;
	}
	if_freenameindex(iff);
	errno = ENXIO;
	return 0;
}

char *
if_indextoname(ifindex, ifname)
	unsigned int ifindex;
	char *ifname; /* at least IF_NAMESIZE */
{
	struct if_nameindex *iff = if_nameindex(), *ifx;
	char *cp, *dp;

	if (iff == NULL) return NULL;
	ifx = iff;
	while (ifx->if_index != 0) {
		if (ifx->if_index == ifindex) {
			cp = ifname;
			dp = ifx->if_name;
			while ((*cp++ = *dp++)) ;
			if_freenameindex(iff);
			return (ifname);
		}
		ifx++;
	}
	if_freenameindex(iff);
	errno = ENXIO;
	return NULL;
}

struct if_nameindex *
if_nameindex()
{
	size_t needed;
	int mib[6], i, ifn = 0, off = 0, hlen;
	char *buf = NULL, *lim, *next, *cp, *ifbuf = NULL;
	struct rt_msghdr *rtm;
	struct if_msghdr *ifm;
	struct sockaddr *sa;
	struct sockaddr_dl *sdl;
	struct if_nameindex *ret = NULL;
	static int ifxs = 64;	/* initial upper limit */
	struct _ifx {
		int if_index;
		int if_off;
	} *ifx = NULL;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;		/* protocol */
	mib[3] = 0;		/* wildcard address family */
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;		/* no flags */
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		return NULL;
	if ((buf = malloc(needed)) == NULL) {
		errno = ENOMEM;
		goto end;
	}
	/* XXX: we may have allocated too much than necessary */
	if ((ifbuf = malloc(needed)) == NULL) {
		errno = ENOMEM;
		goto end;
	}
	if ((ifx = (struct _ifx *)malloc(sizeof(*ifx) * ifxs)) == NULL) {
		errno = ENOMEM;
		goto end;
	}
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
		/* sysctl has set errno */
		goto end;
	}
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (rtm->rtm_version != RTM_VERSION) {
			errno = EPROTONOSUPPORT;
			goto end;
		}
		switch (rtm->rtm_type) {
		case RTM_IFINFO:
			ifm = (struct if_msghdr *)rtm;
			ifx[ifn].if_index = ifm->ifm_index;
			ifx[ifn].if_off = off;
			cp = (char *)(ifm + 1);
			for (i = 1; i; i <<= 1) {
				if (i & ifm->ifm_addrs) {
					sa = (struct sockaddr *)cp;
					if (i == RTA_IFP &&
					    sa->sa_family == AF_LINK) {
						sdl = (struct sockaddr_dl *)sa;
						memcpy(ifbuf + off,
						       sdl->sdl_data,
						       sdl->sdl_nlen);
						off += sdl->sdl_nlen;
						*(ifbuf + off) = '\0';
						off++;
					}
					ADVANCE(cp, sa);
				}
			}
			if (++ifn == ifxs) {
				/* we need more memory */
				struct _ifx *newifx;

				ifxs *= 2;
				if ((newifx = (struct _ifx *)malloc(sizeof(*newifx) * ifxs)) == NULL) {
					errno = ENOMEM;
					goto end;
				}

				/* copy and free old data */
				memcpy(newifx, ifx, (sizeof(*ifx) * ifxs) / 2);
				free(ifx);
				ifx = newifx;
			}
		}
	}
	hlen = sizeof(struct if_nameindex) * (ifn + 1);
	if ((cp = (char *)malloc(hlen + off)) == NULL) {
		errno = ENOMEM;
		goto end;
	}
	bcopy(ifbuf, cp + hlen, off);
	ret = (struct if_nameindex *)cp;
	for (i = 0; i < ifn; i++) {
		ret[i].if_index = ifx[i].if_index;
		ret[i].if_name = cp + hlen + ifx[i].if_off;
	}
	ret[ifn].if_index = 0;
	ret[ifn].if_name = NULL;

  end:
	if (buf) free(buf);
	if (ifbuf) free(ifbuf);
	if (ifx) free(ifx);

	return ret;
}

void if_freenameindex(ptr)
	struct if_nameindex *ptr;
{
	free(ptr);
}
