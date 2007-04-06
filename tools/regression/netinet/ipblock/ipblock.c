/*-
 * Copyright (c) 2007 Bruce M. Simpson
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

/*
 * Test utility for IPv4 multicast source filtering, using UDP.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <pwd.h>
#include <unistd.h>
#include <netdb.h>
#include <libgen.h>
#include <setjmp.h>

#define DEFAULT_PORT		6698
#define BUF_SIZE		2048

static char *progname = NULL;
static jmp_buf jmpbuf;

static void
sighand(int signo __unused)
{
	longjmp(jmpbuf, 1);
}

static void
usage(void)
{

	fprintf(stderr, "IPv4 test program.");
	fprintf(stderr,
"usage: %s [-b filter-ip] [-g multi-group] [-i iface-ip] [-p port] [-r]\n",
	    progname);
	fprintf(stderr, "-b: IPv4 address to filter out\n");
	fprintf(stderr, "-g: IPv4 multicast group to listen on\n");
	fprintf(stderr, "-i: IPv4 address to listen on\n");
	fprintf(stderr, "-p: Set local and remote port (default: %d)\n",
	    DEFAULT_PORT);
	fprintf(stderr, "-r: Set SO_REUSEPORT option on socket\n");

	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	char			 buf[BUF_SIZE];
	struct in_addr		 baddr;
	struct in_addr		 gaddr;
	struct in_addr		 iaddr;
	struct ip_mreq		 mreq;
#ifdef IP_BLOCK_SOURCE
	struct ip_mreq_source	 mreqs;
#endif
	struct sockaddr_in	 from;
	struct sockaddr_in	 laddr;
	int			 ch;
	socklen_t		 fromlen;
	int			 optval;
	int			 portno;
	int			 ret;
	int			 reuseport;
	ssize_t			 rlen;
	int			 s;

	gaddr.s_addr = baddr.s_addr = iaddr.s_addr = INADDR_ANY;
	portno = DEFAULT_PORT;
	reuseport = 0;

#ifndef IP_BLOCK_SOURCE
	fprintf(stderr, "This program requires a version of FreeBSD with\n"
	    "support for source-specific multicast and/or IGMPv3.\n");
	exit(-1);
#endif

	progname = basename(argv[0]);
	while ((ch = getopt(argc, argv, "b:g:i:p:r")) != -1) {
		switch (ch) {
		case 'b':
			baddr.s_addr = inet_addr(optarg);
			break;
		case 'g':
			gaddr.s_addr = inet_addr(optarg);
			break;
		case 'i':
			iaddr.s_addr = inet_addr(optarg);
			break;
		case 'p':
			portno = atoi(optarg);
			break;
		case 'r':
			reuseport = 1;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	if ((baddr.s_addr == INADDR_ANY) || (gaddr.s_addr == INADDR_ANY) ||
	    (iaddr.s_addr == INADDR_ANY))
		usage();

	s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	if (reuseport) {
		optval = 1;
		if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &optval,
		    sizeof(optval)) < 0) {
			perror("setsockopt SO_REUSEPORT");
			close(s);
			exit(EXIT_FAILURE);
		}
	}

	memset(&laddr, 0, sizeof(struct sockaddr_in));
	laddr.sin_family = AF_INET;
	laddr.sin_len = sizeof(struct sockaddr_in);
	laddr.sin_addr.s_addr = INADDR_ANY;
	laddr.sin_port = htons(portno);
	ret = bind(s, (struct sockaddr *)&laddr, sizeof(laddr));
	if (ret == -1) {
		perror("bind");
		close(s);
		exit(EXIT_FAILURE);
	}

	mreq.imr_multiaddr = gaddr;
	mreq.imr_interface = iaddr;
	if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,
	    sizeof(mreq)) < 0) {
		perror("setsockopt IP_ADD_MEMBERSHIP");
		close(s);
		exit(EXIT_FAILURE);
	}

#ifdef IP_BLOCK_SOURCE
	mreqs.imr_multiaddr = gaddr;
	mreqs.imr_interface = iaddr;
	mreqs.imr_sourceaddr = baddr;
	if (setsockopt(s, IPPROTO_IP, IP_BLOCK_SOURCE, &mreqs,
	    sizeof(mreqs)) < 0) {
		perror("setsockopt IP_BLOCK_SOURCE");
		close(s);
		exit(EXIT_FAILURE);
	}
#endif

	signal(SIGINT, sighand);
	while (setjmp(jmpbuf) == 0) {
		fromlen = sizeof(from);
		rlen = recvfrom(s, buf, BUF_SIZE, 0, (struct sockaddr *)&from,
		    &fromlen);
		if (from.sin_addr.s_addr == baddr.s_addr) {
			fprintf(stderr, "WARNING: got packet from blocked %s\n",
			    inet_ntoa(from.sin_addr));
		} else {
			fprintf(stderr, "OK: got packet from %s\n",
			    inet_ntoa(from.sin_addr));
		}
	}

	close(s);

	exit(EXIT_SUCCESS);
}
