/* $FreeBSD$ */
/*	$NetBSD: as.c,v 1.1 2001/11/04 23:14:36 atatat Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Brown.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <err.h>
#include <stdio.h>

#include "as.h"

#define DEFAULT_AS_SERVER "whois.radb.net"
#undef AS_DEBUG_FILE

struct aslookup {
	FILE *as_f;
#ifdef AS_DEBUG_FILE
	FILE *as_debug;
#endif /* AS_DEBUG_FILE */
};

void *
as_setup(server)
	char *server;
{
	struct aslookup *asn;
	struct hostent *he = NULL;
	struct servent *se;
	struct sockaddr_in in;
	FILE *f;
	int s;

	if (server == NULL)
		server = DEFAULT_AS_SERVER;

	(void)memset(&in, 0, sizeof(in));
	in.sin_family = AF_INET;
	in.sin_len = sizeof(in);
	if ((se = getservbyname("whois", "tcp")) == NULL) {
		warnx("warning: whois/tcp service not found");
		in.sin_port = ntohs(43);
	} else
		in.sin_port = se->s_port;

	if (inet_aton(server, &in.sin_addr) == 0 && 
	    ((he = gethostbyname(server)) == NULL ||
	    he->h_addr == NULL)) {
		warnx("%s: %s", server, hstrerror(h_errno));
		return (NULL);
	}

	if ((s = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		warn("socket");
		return (NULL);
	}

	do {
		if (he != NULL) {
			memcpy(&in.sin_addr, he->h_addr, he->h_length);
			he->h_addr_list++;
		}
		if (connect(s, (struct sockaddr *)&in, sizeof(in)) == 0)
			break;
		if (he == NULL || he->h_addr == NULL) {
			close(s);
			s = -1;
			break;
		}
	} while (1);

	if (s == -1) {
		warn("connect");
		return (NULL);
	}

	f = fdopen(s, "r+");
	(void)fprintf(f, "!!\n");
	(void)fflush(f);

	asn = malloc(sizeof(struct aslookup));
	if (asn == NULL)
		(void)fclose(f);
	else
		asn->as_f = f;

#ifdef AS_DEBUG_FILE
	asn->as_debug = fopen(AS_DEBUG_FILE, "w");
	if (asn->as_debug) {
		(void)fprintf(asn->as_debug, ">> !!\n");
		(void)fflush(asn->as_debug);
	}
#endif /* AS_DEBUG_FILE */

	return (asn);
}

int
as_lookup(_asn, addr)
	void *_asn;
	struct in_addr *addr;
{
	struct aslookup *asn = _asn;
	char buf[1024];
	int as, rc, dlen;

	as = rc = dlen = 0;
	(void)fprintf(asn->as_f, "!r%s/32,l\n", inet_ntoa(*addr));
	(void)fflush(asn->as_f);

#ifdef AS_DEBUG_FILE
	if (asn->as_debug) {
		(void)fprintf(asn->as_debug, ">> !r%s/32,l\n",
		     inet_ntoa(*addr));
		(void)fflush(asn->as_debug);
	}
#endif /* AS_DEBUG_FILE */

	while (fgets(buf, sizeof(buf), asn->as_f) != NULL) {
		buf[sizeof(buf) - 1] = '\0';

#ifdef AS_DEBUG_FILE
		if (asn->as_debug) {
			(void)fprintf(asn->as_debug, "<< %s", buf);
			(void)fflush(asn->as_debug);
		}
#endif /* AS_DEBUG_FILE */

		if (rc == 0) {
			rc = buf[0];
			switch (rc) {
			    case 'A':
				/* A - followed by # bytes of answer */
				sscanf(buf, "A%d\n", &dlen);
#ifdef AS_DEBUG_FILE
				if (asn->as_debug) {
					(void)fprintf(asn->as_debug,
					     "dlen: %d\n", dlen);
					(void)fflush(asn->as_debug);
				}
#endif /* AS_DEBUG_FILE */
				break;
			    case 'C':	
			    case 'D':
			    case 'E':
			    case 'F':
				/* C - no data returned */
				/* D - key not found */
				/* E - multiple copies of key */
				/* F - some other error */
				break;
			}
			if (rc == 'A')
				/* skip to next input line */
				continue;
		}

		if (dlen == 0)
			/* out of data, next char read is end code */
			rc = buf[0];
		if (rc != 'A')
			/* either an error off the bat, or a done code */
			break;

		/* data received, thank you */
		dlen -= strlen(buf);

		/* origin line is the interesting bit */
		if (as == 0 && strncasecmp(buf, "origin:", 7) == 0) {
			sscanf(buf + 7, " AS%d", &as);
#ifdef AS_DEBUG_FILE
			if (asn->as_debug) {
				(void)fprintf(asn->as_debug, "as: %d\n", as);
				(void)fflush(asn->as_debug);
			}
#endif /* AS_DEBUG_FILE */
		}
	}

	return (as);
}

void
as_shutdown(_asn)
	void *_asn;
{
	struct aslookup *asn = _asn;

	(void)fprintf(asn->as_f, "!q\n");
	(void)fclose(asn->as_f);

#ifdef AS_DEBUG_FILE
	if (asn->as_debug) {
		(void)fprintf(asn->as_debug, ">> !q\n");
		(void)fclose(asn->as_debug);
	}
#endif /* AS_DEBUG_FILE */

	free(asn);
}
