/*-
 * Copyright (c) 2018 Christian S.J. Peron
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <bsm/audit.h>
#include <bsm/libbsm.h>

#include <netinet/in.h>

#include <err.h>
#include <netdb.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
usage(char *prog)
{
	(void)fprintf(stderr,
    "usage: %s [-46U] [-a auid] [-m mask] [-p port] [-s source] command ...\n",
	    prog);
	exit(1);
}

int
main(int argc, char *argv [])
{
	struct sockaddr_in6 *sin6;
	struct sockaddr_in *sin;
	struct addrinfo hints;
	auditinfo_addr_t aia;
	char *aflag, *mflag, *sflag, *prog;
	dev_t term_port;
	uint32_t term_type;
	int ch, error;
	bool Uflag;

	aflag = mflag = sflag = NULL;
	Uflag = false;

	prog = argv[0];
	bzero(&aia, sizeof(aia));
	bzero(&hints, sizeof(hints));
	term_type = AU_IPv4;
	hints.ai_family = PF_UNSPEC;
	while ((ch = getopt(argc, argv, "46a:m:p:s:U")) != -1)
		switch (ch) {
		case '4':
			hints.ai_family = PF_INET;
			break;
		case '6':
			hints.ai_family = PF_INET6;
			break;
		case 'a':
			aflag = optarg;
			break;
		case 'm':
			mflag = optarg;
			break;
		case 'p':
			term_port = htons(atoi(optarg));
			break;
		case 's':
			sflag = optarg;
			break;
		case 'U':
			Uflag = true;
			break;
		default:
			usage(prog);
			/* NOT REACHED */
		}
	argc -= optind;
	argv += optind;
	if (argc == 0)
		usage(prog);

	if (Uflag) {
		if (getaudit_addr(&aia, sizeof(aia)) < 0)
			err(1, "getaudit_addr");
	}
	if (aflag) {
		struct passwd *pwd;

		pwd = getpwnam(aflag);
		if (pwd == NULL) {
			char *r;

			aia.ai_auid = strtoul(aflag, &r, 10);
			if (*r != '\0')
				errx(1, "%s: invalid user", aflag);
		} else
			aia.ai_auid = pwd->pw_uid;
	}
	if (mflag) {
		if (getauditflagsbin(mflag, &aia.ai_mask) < 0)
			err(1, "getauditflagsbin");
	}
	if (sflag) {
		struct addrinfo *res;

		error = getaddrinfo(sflag, NULL, &hints, &res);
		if (error)
			errx(1, "%s", gai_strerror(error));
		switch (res->ai_family) {
		case PF_INET6:
			sin6 = (struct sockaddr_in6 *)(void *)res->ai_addr;
			bcopy(&sin6->sin6_addr.s6_addr,
			    &aia.ai_termid.at_addr[0],
			    sizeof(struct in6_addr));
			term_type = AU_IPv6;
			break;
		case PF_INET:
			sin = (struct sockaddr_in *)(void *)res->ai_addr;
			bcopy(&sin->sin_addr.s_addr,
			    &aia.ai_termid.at_addr[0],
			    sizeof(struct in_addr));
			term_type = AU_IPv4;
			break;
		}
	}
	if (!Uflag || sflag) {
		aia.ai_termid.at_port = term_port;
		aia.ai_termid.at_type = term_type;
	}
	if (setaudit_addr(&aia, sizeof(aia)) < 0)
		err(1, "setaudit_addr");
	(void)execvp(*argv, argv);
	err(1, "%s", *argv);
}
