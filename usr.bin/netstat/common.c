/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1983, 1988, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <libutil.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <err.h>
#include <libxo/xo.h>
#include "netstat.h"
#include "common.h"

const char *
fmt_flags(const struct bits *p, int f)
{
	static char name[33];
	char *flags;

	for (flags = name; p->b_mask; p++)
		if (p->b_mask & f)
			*flags++ = p->b_val;
	*flags = '\0';
	return (name);
}

void
print_flags_generic(int flags, const struct bits *pbits, const char *format,
    const char *tag_name)
{
	const struct bits *p;
	char tag_fmt[64];

	xo_emit(format, fmt_flags(pbits, flags));

	snprintf(tag_fmt, sizeof(tag_fmt), "{le:%s/%%s}", tag_name);
	xo_open_list(tag_name);
	for (p = pbits; p->b_mask; p++)
		if (p->b_mask & flags)
			xo_emit(tag_fmt, p->b_name);
	xo_close_list(tag_name);
}

struct ifmap_entry *
prepare_ifmap(size_t *pifmap_size)
{
	int ifindex = 0, size;
	struct ifaddrs *ifap, *ifa;
	struct sockaddr_dl *sdl;

	struct ifmap_entry *ifmap = NULL;
	int ifmap_size = 0;

	/*
	 * Retrieve interface list at first
	 * since we need #ifindex -> if_xname match
	 */
	if (getifaddrs(&ifap) != 0)
		err(EX_OSERR, "getifaddrs");

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {

		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;

		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		ifindex = sdl->sdl_index;

		if (ifindex >= ifmap_size) {
			size = roundup(ifindex + 1, 32) *
			    sizeof(struct ifmap_entry);
			if ((ifmap = realloc(ifmap, size)) == NULL)
				errx(2, "realloc(%d) failed", size);
			memset(&ifmap[ifmap_size], 0,
			    size - ifmap_size *
			    sizeof(struct ifmap_entry));

			ifmap_size = roundup(ifindex + 1, 32);
		}

		if (*ifmap[ifindex].ifname != '\0')
			continue;

		strlcpy(ifmap[ifindex].ifname, ifa->ifa_name, IFNAMSIZ);
	}

	freeifaddrs(ifap);

	*pifmap_size = ifmap_size;

	return (ifmap);
}

