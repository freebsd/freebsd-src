/*-
 * Copyright (c) 1982, 1986, 1993
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

char copyright[] =
"@(#) Copyright (c) 1982, 1986, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";

#if !defined(lint) && !defined(sgi) && !defined(__NetBSD__)
static char sccsid[] = "@(#)query.c	8.1 (Berkeley) 6/5/93";
#elif defined(__NetBSD__)
static char rcsid[] = "$NetBSD$";
#endif
#ident "$Revision: 1.1.1.1 $"

#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#define RIPVERSION RIPv2
#include <protocols/routed.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef sgi
#include <strings.h>
#include <bstring.h>
#endif

#ifndef sgi
#define _HAVE_SIN_LEN
#endif

#define	WTIME	15		/* Time to wait for all responses */
#define	STIME	(250*1000)	/* usec to wait for another response */

int	s;

union {
	struct rip rip;
	char	packet[MAXPACKETSIZE+MAXPATHLEN];
} omsg_buf;
#define OMSG omsg_buf.rip
int omsg_len = sizeof(struct rip);

union {
	struct	rip rip;
	char	packet[MAXPACKETSIZE+1024];
	} imsg_buf;
#define IMSG imsg_buf.rip

int	nflag;				/* numbers, no names */
int	pflag;				/* play the `gated` game */
int	ripv2 = 1;			/* use RIP version 2 */
int	wtime = WTIME;
int	rflag;				/* 1=ask about a particular route */
int	trace;
int	not_trace;

struct timeval sent;			/* when query sent */

static void rip_input(struct sockaddr_in*, int);
static int out(char *);
static void trace_loop(char *argv[]);
static void query_loop(char *argv[], int);
static int getnet(char *, struct netinfo *);
static u_int std_mask(u_int);


int
main(int argc,
     char *argv[])
{
	int ch, bsize;
	char *p, *options, *value;

	OMSG.rip_nets[0].n_dst = RIP_DEFAULT;
	OMSG.rip_nets[0].n_family = RIP_AF_UNSPEC;
	OMSG.rip_nets[0].n_metric = htonl(HOPCNT_INFINITY);

	while ((ch = getopt(argc, argv, "np1w:r:t:")) != EOF)
		switch (ch) {
		case 'n':
			not_trace = 1;
			nflag = 1;
			break;

		case 'p':
			not_trace = 1;
			pflag = 1;
			break;

		case '1':
			ripv2 = 0;
			break;

		case 'w':
			not_trace = 1;
			wtime = (int)strtoul(optarg, &p, 0);
			if (*p != '\0'
			    || wtime <= 0)
				goto usage;
			break;

		case 'r':
			not_trace = 1;
			if (rflag)
				goto usage;
			rflag = getnet(optarg, &OMSG.rip_nets[0]);
			if (!rflag) {
				struct hostent *hp = gethostbyname(optarg);
				if (hp == 0) {
					fprintf(stderr, "rtquery: %s:", optarg);
					herror(0);
					exit(1);
				}
				bcopy(hp->h_addr, &OMSG.rip_nets[0].n_dst,
				      sizeof(OMSG.rip_nets[0].n_dst));
				OMSG.rip_nets[0].n_family = RIP_AF_INET;
				OMSG.rip_nets[0].n_mask = -1;
				rflag = 1;
			}
			break;

		case 't':
			trace = 1;
			options = optarg;
			while (*options != '\0') {
				char *traceopts[] = {
#				    define TRACE_ON	0
					"on",
#				    define TRACE_MORE	1
					"more",
#				    define TRACE_OFF	2
					"off",
#				    define TRACE_DUMP	3
					"dump",
					0
				};
				switch (getsubopt(&options,traceopts,&value)) {
				case TRACE_ON:
					OMSG.rip_cmd = RIPCMD_TRACEON;
					if (!value
					    || strlen(value) > MAXPATHLEN)
						goto usage;
					break;
				case TRACE_MORE:
					if (value)
						goto usage;
					OMSG.rip_cmd = RIPCMD_TRACEON;
					value = "";
					break;
				case TRACE_OFF:
					if (value)
						goto usage;
					OMSG.rip_cmd = RIPCMD_TRACEOFF;
					value = "";
					break;
				case TRACE_DUMP:
					if (value)
						goto usage;
					OMSG.rip_cmd = RIPCMD_TRACEON;
					value = "dump/../table";
					break;
				default:
					goto usage;
				}
				strcpy((char*)OMSG.rip_tracefile, value);
				omsg_len += strlen(value) - sizeof(OMSG.ripun);
			}
			break;

		default:
			goto usage;
	}
	argv += optind;
	argc -= optind;
	if ((not_trace && trace) || argc == 0) {
usage:			fprintf(stderr, "%s\n%s\n",
		"usage: rtquery [-np1] [-r addr] [-w timeout] host ...",
		"       rtquery [-t op] host ...");
		exit(1);
	}

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket");
		exit(2);
	}

	/* be prepared to receive a lot of routes */
	for (bsize = 127*1024; ; bsize -= 1024) {
		if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
			       &bsize, sizeof(bsize)) == 0)
			break;
		if (bsize <= 4*1024) {
			perror("setsockopt SO_RCVBUF");
			break;
		}
	}

	if (trace)
		trace_loop(argv);
	else
		query_loop(argv, argc);
	/* NOTREACHED */
}


/* tell the target hosts about tracing
 */
static void
trace_loop(char *argv[])
{
	struct sockaddr_in myaddr;
	int res;

	if (geteuid() != 0) {
		(void)fprintf(stderr, "-t requires UID 0\n");
		exit(1);
	}

	if (ripv2) {
		OMSG.rip_vers = RIPv2;
	} else {
		OMSG.rip_vers = RIPv1;
	}

	bzero(&myaddr, sizeof(myaddr));
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

	res = 1;
	while (*argv != 0) {
		if (out(*argv++) <= 0)
			res = 0;
	}
	exit(res);
}


/* query all of the listed hosts
 */
static void
query_loop(char *argv[], int argc)
{
	struct seen {
		struct seen *next;
		struct in_addr addr;
	} *seen, *sp;
	int answered = 0;
	int cc;
	fd_set bits;
	struct timeval now, delay;
	struct sockaddr_in from;
	int fromlen;


	OMSG.rip_cmd = (pflag) ? RIPCMD_POLL : RIPCMD_REQUEST;
	if (ripv2) {
		OMSG.rip_vers = RIPv2;
	} else {
		OMSG.rip_vers = RIPv1;
		OMSG.rip_nets[0].n_mask = 0;
	}

	/* ask the first (valid) host */
	seen = 0;
	while (0 > out(*argv++)) {
		if (*argv == 0)
			exit(-1);
		answered++;
	}

	FD_ZERO(&bits);
	for (;;) {
		FD_SET(s, &bits);
		delay.tv_sec = 0;
		delay.tv_usec = STIME;
		cc = select(s+1, &bits, 0,0, &delay);
		if (cc > 0) {
			fromlen = sizeof(from);
			cc = recvfrom(s, imsg_buf.packet,
				      sizeof(imsg_buf.packet), 0,
				      (struct sockaddr *)&from, &fromlen);
			if (cc < 0) {
				perror("recvfrom");
				exit(1);
			}
			/* count the distinct responding hosts.
			 * You cannot match responding hosts with
			 * addresses to which queries were transmitted,
			 * because a router might respond with a
			 * different source address.
			 */
			for (sp = seen; sp != 0; sp = sp->next) {
				if (sp->addr.s_addr == from.sin_addr.s_addr)
					break;
			}
			if (sp == 0) {
				sp = malloc(sizeof(*sp));
				sp->addr = from.sin_addr;
				sp->next = seen;
				seen = sp;
				answered++;
			}

			rip_input(&from, cc);
			continue;
		}

		if (cc < 0) {
			if ( errno == EINTR)
				continue;
			perror("select");
			exit(1);
		}

		/* After a pause in responses, probe another host.
		 * This reduces the intermingling of answers.
		 */
		while (*argv != 0 && 0 > out(*argv++))
			answered++;

		/* continue until no more packets arrive
		 * or we have heard from all hosts
		 */
		if (answered >= argc)
			break;

		/* or until we have waited a long time
		 */
		if (gettimeofday(&now, 0) < 0) {
			perror("gettimeofday(now)");
			exit(1);
		}
		if (sent.tv_sec + wtime <= now.tv_sec)
			break;
	}

	/* fail if there was no answer */
	exit (answered >= argc ? 0 : 1);
}


/* sent do one host
 */
static int
out(char *host)
{
	struct sockaddr_in router;
	struct hostent *hp;

	if (gettimeofday(&sent, 0) < 0) {
		perror("gettimeofday(sent)");
		return -1;
	}

	bzero(&router, sizeof(router));
	router.sin_family = AF_INET;
#ifdef _HAVE_SIN_LEN
	router.sin_len = sizeof(router);
#endif
	if (!inet_aton(host, &router.sin_addr)) {
		hp = gethostbyname(host);
		if (hp == 0) {
			herror(host);
			return -1;
		}
		bcopy(hp->h_addr, &router.sin_addr, sizeof(router.sin_addr));
	}
	router.sin_port = htons(RIP_PORT);

	if (sendto(s, &omsg_buf, omsg_len, 0,
		   (struct sockaddr *)&router, sizeof(router)) < 0) {
		perror(host);
		return -1;
	}

	return 0;
}


/*
 * Handle an incoming RIP packet.
 */
static void
rip_input(struct sockaddr_in *from,
	  int size)
{
	struct netinfo *n, *lim;
	struct in_addr in;
	char *name;
	char net_buf[80];
	u_int mask, dmask;
	char *sp;
	int i;
	struct hostent *hp;
	struct netent *np;
	struct netauth *a;


	if (nflag) {
		printf("%s:", inet_ntoa(from->sin_addr));
	} else {
		hp = gethostbyaddr((char*)&from->sin_addr,
				   sizeof(struct in_addr), AF_INET);
		if (hp == 0) {
			printf("%s:",
			       inet_ntoa(from->sin_addr));
		} else {
			printf("%s (%s):", hp->h_name,
			       inet_ntoa(from->sin_addr));
		}
	}
	if (IMSG.rip_cmd != RIPCMD_RESPONSE) {
		printf("\n    unexpected response type %d\n", IMSG.rip_cmd);
		return;
	}
	printf(" RIPv%d%s %d bytes\n", IMSG.rip_vers,
	       (IMSG.rip_vers != RIPv1 && IMSG.rip_vers != RIPv2) ? " ?" : "",
	       size);
	if (size > MAXPACKETSIZE) {
		if (size > sizeof(imsg_buf) - sizeof(*n)) {
			printf("       at least %d bytes too long\n",
			       size-MAXPACKETSIZE);
			size = sizeof(imsg_buf) - sizeof(*n);
		} else {
			printf("       %d bytes too long\n",
			       size-MAXPACKETSIZE);
		}
	} else if (size%sizeof(*n) != sizeof(struct rip)%sizeof(*n)) {
		printf("    response of bad length=%d\n", size);
	}

	n = IMSG.rip_nets;
	lim = (struct netinfo *)((char*)n + size) - 1;
	for (; n <= lim; n++) {
		name = "";
		if (n->n_family == RIP_AF_INET) {
			in.s_addr = n->n_dst;
			(void)strcpy(net_buf, inet_ntoa(in));

			mask = ntohl(n->n_mask);
			dmask = mask & -mask;
			if (mask != 0) {
				sp = &net_buf[strlen(net_buf)];
				if (IMSG.rip_vers == RIPv1) {
					(void)sprintf(sp," mask=%#x ? ",mask);
					mask = 0;
				} else if (mask + dmask == 0) {
					for (i = 0;
					     (i != 32
					      && ((1<<i)&mask) == 0);
					     i++)
						continue;
					(void)sprintf(sp, "/%d",32-i);
				} else {
					(void)sprintf(sp," (mask %#x)", mask);
				}
			}

			if (!nflag) {
				if (mask == 0) {
					mask = std_mask(in.s_addr);
					if ((ntohl(in.s_addr) & ~mask) != 0)
						mask = 0;
				}
				/* Without a netmask, do not worry about
				 * whether the destination is a host or a
				 * network. Try both and use the first name
				 * we get.
				 *
				 * If we have a netmask we can make a
				 * good guess.
				 */
				if ((in.s_addr & ~mask) == 0) {
					np = getnetbyaddr((long)in.s_addr,
							  AF_INET);
					if (np != 0)
						name = np->n_name;
					else if (in.s_addr == 0)
						name = "default";
				}
				if (name[0] == '\0'
				    && ((in.s_addr & ~mask) != 0
					|| mask == 0xffffffff)) {
					hp = gethostbyaddr((char*)&in,
							   sizeof(in),
							   AF_INET);
					if (hp != 0)
						name = hp->h_name;
				}
			}

		} else if (n->n_family == RIP_AF_AUTH) {
			a = (struct netauth*)n;
			(void)printf("    authentication type %d: ",
				     a->a_type);
			for (i = 0; i < sizeof(a->au.au_pw); i++)
				(void)printf("%02x ", a->au.au_pw[i]);
			putc('\n', stdout);
			continue;

		} else {
			(void)sprintf(net_buf, "(af %#x) %d.%d.%d.%d",
				      ntohs(n->n_family),
				      (char)(n->n_dst >> 24),
				      (char)(n->n_dst >> 16),
				      (char)(n->n_dst >> 8),
				      (char)n->n_dst);
		}

		(void)printf("  %-18s metric %2d %-10s",
			     net_buf, ntohl(n->n_metric), name);

		if (n->n_nhop != 0) {
			in.s_addr = n->n_nhop;
			if (nflag)
				hp = 0;
			else
				hp = gethostbyaddr((char*)&in, sizeof(in),
						   AF_INET);
			(void)printf(" nhop=%-15s%s",
				     (hp != 0) ? hp->h_name : inet_ntoa(in),
				     (IMSG.rip_vers == RIPv1) ? " ?" : "");
		}
		if (n->n_tag != 0)
			(void)printf(" tag=%#x%s", n->n_tag,
				     (IMSG.rip_vers == RIPv1) ? " ?" : "");
		putc('\n', stdout);
	}
}


/* Return the classical netmask for an IP address.
 */
static u_int
std_mask(u_int addr)			/* in network order */
{
	NTOHL(addr);			/* was a host, not a network */

	if (addr == 0)			/* default route has mask 0 */
		return 0;
	if (IN_CLASSA(addr))
		return IN_CLASSA_NET;
	if (IN_CLASSB(addr))
		return IN_CLASSB_NET;
	return IN_CLASSC_NET;
}


/* get a network number as a name or a number, with an optional "/xx"
 * netmask.
 */
static int				/* 0=bad */
getnet(char *name,
       struct netinfo *rt)
{
	int i;
	struct netent *nentp;
	u_int mask;
	struct in_addr in;
	char hname[MAXHOSTNAMELEN+1];
	char *mname, *p;


	/* Detect and separate "1.2.3.4/24"
	 */
	if (0 != (mname = rindex(name,'/'))) {
		i = (int)(mname - name);
		if (i > sizeof(hname)-1)	/* name too long */
			return 0;
		bcopy(name, hname, i);
		hname[i] = '\0';
		mname++;
		name = hname;
	}

	nentp = getnetbyname(name);
	if (nentp != 0) {
		in.s_addr = nentp->n_net;
	} else if (inet_aton(name, &in) == 1) {
		NTOHL(in.s_addr);
	} else {
		return 0;
	}

	if (mname == 0) {
		mask = std_mask(in.s_addr);
		if ((~mask & in.s_addr) != 0)
			mask = 0xffffffff;
	} else {
		mask = (u_int)strtoul(mname, &p, 0);
		if (*p != '\0' || mask > 32)
			return 0;
		mask = 0xffffffff << (32-mask);
	}

	rt->n_dst = htonl(in.s_addr);
	rt->n_family = RIP_AF_INET;
	rt->n_mask = htonl(mask);
	return 1;
}
