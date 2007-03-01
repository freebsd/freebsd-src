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
 * Test utility for IPv4 broadcast sockets.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

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

#define DEFAULT_PORT		6698
#define DEFAULT_PAYLOAD_SIZE	24
#define DEFAULT_TTL		1
#define MY_CMSG_SIZE	CMSG_SPACE(sizeof(struct in_addr))

static char *progname = NULL;

static void
usage(void)
{

	fprintf(stderr,
"usage: %s [-1] [-b] [-B] [-d] [-l len] [-p port] [-r] [-s srcaddr] [-t ttl]\n"
"    <dest>\n",
	    progname);
	fprintf(stderr, "IPv4 broadcast test program. Sends a %d byte UDP "
	        "datagram to <dest>:<port>.\n", DEFAULT_PAYLOAD_SIZE);
	fprintf(stderr, "-1: Set IP_ONESBCAST\n");
	fprintf(stderr, "-b: bind socket to INADDR_ANY:<sport>\n");
	fprintf(stderr, "-B: Set SO_BROADCAST\n");
	fprintf(stderr, "-d: Set SO_DONTROUTE\n");
#if 0
	fprintf(stderr, "-r: Fill datagram with random bytes\n");
#endif
	fprintf(stderr, "-l: Set payload size to <len>\n");
	fprintf(stderr, "-p: Set source and destination port (default: %d)\n",
	    DEFAULT_PORT);
	fprintf(stderr, "-s: Set IP_SENDSRCADDR to <srcaddr>\n");
	fprintf(stderr, "-t: Set IP_TTL to <ttl>\n");

	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	char			*buf;
	char			 cmsgbuf[MY_CMSG_SIZE];
	struct iovec		 iov[1];
	struct msghdr		 msg;
	struct sockaddr_in	 dsin;
	struct cmsghdr		*cmsgp;
	struct in_addr		 dstaddr;
	struct in_addr		*srcaddrp;
	char			*srcaddr_s;
	int			 ch;
	int			 dobind;
	int			 dobroadcast;
	int			 dontroute;
	int			 doonesbcast;
	int			 dorandom;
	size_t			 buflen;
	ssize_t			 nbytes;
	int			 portno;
	int			 ret;
	int			 s;
	socklen_t		 soptlen;
	int			 soptval;
	int			 ttl;

	dobind = 0;
	dobroadcast = 0;
	dontroute = 0;
	doonesbcast = 0;
	dorandom = 0;

	dstaddr.s_addr = INADDR_ANY;
	srcaddr_s = NULL;
	portno = DEFAULT_PORT;
	ttl = DEFAULT_TTL;

	buf = NULL;
	buflen = DEFAULT_PAYLOAD_SIZE;

	progname = basename(argv[0]);
	while ((ch = getopt(argc, argv, "1bBdl:p:rs:t:")) != -1) {
		switch (ch) {
		case '1':
			doonesbcast = 1;
			break;
		case 'b':
			dobind = 1;
			break;
		case 'B':
			dobroadcast = 1;
			break;
		case 'd':
			dontroute = 1;
			break;
		case 'l':
			buflen = atoi(optarg);
			break;
		case 'p':
			portno = atoi(optarg);
			break;
		case 'r':
			dorandom = 1;
			break;
		case 's':
			srcaddr_s = optarg;
			break;
		case 't':
			ttl = atoi(optarg);
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();
	if (argv[0] == NULL || inet_aton(argv[0], &dstaddr) == 0)
		usage();
	s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	if (dontroute) {
		soptval = 1;
		soptlen = sizeof(soptval);
		ret = setsockopt(s, SOL_SOCKET, SO_DONTROUTE, &soptval,
		    soptlen);
		if (ret == -1) {
			perror("setsockopt SO_DONTROUTE");
			close(s);
			exit(EXIT_FAILURE);
		}
	}

	if (dobroadcast) {
		soptval = 1;
		soptlen = sizeof(soptval);
		ret = setsockopt(s, SOL_SOCKET, SO_BROADCAST, &soptval,
		    soptlen);
		if (ret == -1) {
			perror("setsockopt SO_BROADCAST");
			close(s);
			exit(EXIT_FAILURE);
		}
	}

	soptval = ttl;
	soptlen = sizeof(soptval);
	ret = setsockopt(s, IPPROTO_IP, IP_TTL, &soptval, soptlen);
	if (ret == -1) {
		perror("setsockopt IPPROTO_IP IP_TTL");
		close(s);
		exit(EXIT_FAILURE);
	}

	if (doonesbcast) {
		soptval = 1;
		soptlen = sizeof(soptval);
		ret = setsockopt(s, IPPROTO_IP, IP_ONESBCAST, &soptval,
		    soptlen);
		if (ret == -1) {
			perror("setsockopt IP_ONESBCAST");
			close(s);
			exit(EXIT_FAILURE);
		}
	}

	if (dobind) {
		memset(&dsin, 0, sizeof(struct sockaddr_in));
		dsin.sin_family = AF_INET;
		dsin.sin_len = sizeof(struct sockaddr_in);
		dsin.sin_addr.s_addr = INADDR_ANY;
		dsin.sin_port = htons(portno);
		ret = bind(s, (struct sockaddr *)&dsin, sizeof(dsin));
		if (ret == -1) {
			perror("bind");
			close(s);
			exit(EXIT_FAILURE);
		}
	}

	memset(&dsin, 0, sizeof(struct sockaddr_in));
	dsin.sin_family = AF_INET;
	dsin.sin_len = sizeof(struct sockaddr_in);
	dsin.sin_addr.s_addr = dstaddr.s_addr;
	dsin.sin_port = htons(portno);

	buf = malloc(buflen);
	if (buf == NULL) {
		perror("malloc");
		close(s);
		exit(EXIT_FAILURE);
	}
	memset(iov, 0, sizeof(iov));
	iov[0].iov_base = buf;
	iov[0].iov_len = buflen;

	memset(&msg, 0, sizeof(struct msghdr));
	msg.msg_name = &dsin;
	msg.msg_namelen = sizeof(dsin);
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	if (srcaddr_s != NULL) {
		memset(cmsgbuf, 0, MY_CMSG_SIZE);
		msg.msg_control = cmsgbuf;
		msg.msg_controllen = sizeof(cmsgbuf);
		cmsgp = CMSG_FIRSTHDR(&msg);
		cmsgp->cmsg_len = CMSG_LEN(sizeof(struct in_addr));
		cmsgp->cmsg_level = IPPROTO_IP;
		cmsgp->cmsg_type = IP_SENDSRCADDR;
		srcaddrp = (struct in_addr *)CMSG_DATA(cmsgp);
		srcaddrp->s_addr = inet_addr(srcaddr_s);
	}

	nbytes = sendmsg(s, &msg, (dontroute ? MSG_DONTROUTE : 0));
	if (nbytes == -1) {
		perror("sendmsg");
		close(s);
		exit(EXIT_FAILURE);
	}

	close(s);

	exit(EXIT_SUCCESS);
}
