/*-
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1983, 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)trace.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#ifdef sgi
#include <bstring.h>
#endif
#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <protocols/routed.h>
#include <arpa/inet.h>

#ifndef sgi
#define _HAVE_SIN_LEN
#endif

struct	sockaddr_in myaddr;
char	packet[MAXPACKETSIZE];

int
main(int argc,
     char **argv)
{
	int size, s;
	struct sockaddr_in router;
	char *tgt;
	register struct rip *msg = (struct rip *)packet;
	struct hostent *hp;

	if (argc < 2) {
usage:
		printf("usage: on filename host1 host2 ...\n"
		       "   or: off host1 host2 ...\n");
		exit(1);
	}
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket");
		exit(2);
	}
	myaddr.sin_family = AF_INET;
#ifdef _HAVE_SIN_LEN
	myaddr.sin_len = sizeof(myaddr);
#endif
	myaddr.sin_port = htons(IPPORT_RESERVED-1);
	while (bind(s, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		if (errno != EADDRINUSE
		    || myaddr.sin_port == 0) {
			perror("bind");
			exit(2);
		}
		myaddr.sin_port = htons(ntohs(myaddr.sin_port)-1);
	}

	msg->rip_vers = RIPVERSION;
	size = sizeof(int);

	argv++, argc--;
	if (!strcmp(*argv, "on")) {
		msg->rip_cmd = RIPCMD_TRACEON;
		if (--argc <= 1)
			goto usage;
		strcpy(msg->rip_tracefile, *++argv);
		size += strlen(msg->rip_tracefile);

	} else if (!strcmp(*argv, "off")) {
		msg->rip_cmd = RIPCMD_TRACEOFF;

	} else {
		goto usage;
	}
	argv++, argc--;

	bzero(&router, sizeof(router));
	router.sin_family = AF_INET;
#ifdef _HAVE_SIN_LEN
	router.sin_len = sizeof(router);
#endif
	router.sin_port = htons(RIP_PORT);

	do {
		tgt = argc > 0 ? *argv++ : "localhost";
		router.sin_family = AF_INET;
		router.sin_addr.s_addr = inet_addr(tgt);
		if (router.sin_addr.s_addr == -1) {
			hp = gethostbyname(tgt);
			if (hp == 0) {
				herror(tgt);
				continue;
			}
			bcopy(hp->h_addr, &router.sin_addr, hp->h_length);
		}
		if (sendto(s, packet, size, 0,
			   (struct sockaddr *)&router, sizeof(router)) < 0)
			perror(*argv);
	} while (--argc > 0);

	return 0;
}
