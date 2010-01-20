/*-
 * Copyright (c) 2007 Bruce M. Simpson.
 * Copyright (c) 2000 Wilbert De Graaf.
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
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Diagnostic and test utility for IPv4 multicast sockets.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/ethernet.h>
#include <netinet/in.h>

#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <err.h>
#include <unistd.h>

/* The following two socket options are private to the kernel and libc. */

#ifndef IP_SETMSFILTER
#define IP_SETMSFILTER 74 /* atomically set filter list */
#endif
#ifndef IP_GETMSFILTER
#define IP_GETMSFILTER 75 /* get filter list */
#endif

static void	process_file(char *, int);
static void	process_cmd(char*, int, FILE *fp);
static void	usage(void);
#ifdef WITH_IGMPV3
static int	inaddr_cmp(const void *a, const void *b);
#endif

#define	MAX_ADDRS	20
#define	STR_SIZE	20
#define	LINE_LENGTH	80

int
main(int argc, char **argv)
{
	char	 line[LINE_LENGTH];
	char	*p;
	int	 i, s;

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == -1)
		err(1, "can't open socket");

	if (argc < 2) {
		if (isatty(STDIN_FILENO)) {
			printf("multicast membership test program; "
			    "enter ? for list of commands\n");
		}
		do {
			if (fgets(line, sizeof(line), stdin) != NULL) {
				if (line[0] != 'f')
					process_cmd(line, s, stdin);
				else {
					/* Get the filename */
					for (i = 1; isblank(line[i]); i++);
					if ((p = (char*)strchr(line, '\n'))
					    != NULL)
						*p = '\0';
					process_file(&line[i], s);
				}
			}
		} while (!feof(stdin));
	} else {
		for (i = 1; i < argc; i++) {
			process_file(argv[i], s);
		}
	}

	exit (0);
}

static void
process_file(char *fname, int s)
{
	char line[80];
	FILE *fp;
	char *lineptr;

	fp = fopen(fname, "r");
	if (fp == NULL) {
		warn("fopen");
		return;
	}

	/* Skip comments and empty lines. */
	while (fgets(line, sizeof(line), fp) != NULL) {
		lineptr = line;
		while (isblank(*lineptr))
			lineptr++;
		if (*lineptr != '#' && *lineptr != '\n')
			process_cmd(lineptr, s, fp);
	}

	fclose(fp);
}

static void
process_cmd(char *cmd, int s, FILE *fp __unused)
{
	char			 str1[STR_SIZE];
	char			 str2[STR_SIZE];
	char			 str3[STR_SIZE];
#ifdef WITH_IGMPV3
	char			 filtbuf[IP_MSFILTER_SIZE(MAX_ADDRS)];
#endif
	struct ifreq		 ifr;
	struct ip_mreq		 imr;
	struct ip_mreq_source	 imrs;
#ifdef WITH_IGMPV3
	struct ip_msfilter	*imsfp;
#endif
	char			*line;
	int			 n, opt, f, flags;

	line = cmd;
	while (isblank(*++line))
		;	/* Skip whitespace. */

	switch (*cmd) {
	case '?':
		usage();
		break;

	case 'q':
		close(s);
		exit(0);

	case 's':
		if ((sscanf(line, "%d", &n) != 1) || (n < 1)) {
			printf("-1\n");
			break;
		}
		sleep(n);
		printf("ok\n");
		break;

	case 'j':
	case 'l':
		sscanf(line, "%s %s", str1, str2);
		if (((imr.imr_multiaddr.s_addr = inet_addr(str1)) ==
		    INADDR_NONE) ||
		    ((imr.imr_interface.s_addr = inet_addr(str2)) ==
		    INADDR_NONE)) {
			printf("-1\n");
			break;
		}
		opt = (*cmd == 'j') ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP;
		if (setsockopt( s, IPPROTO_IP, opt, &imr,
		    sizeof(imr)) != 0)
			warn("setsockopt IP_ADD_MEMBERSHIP/IP_DROP_MEMBERSHIP");
		else
			printf("ok\n");
		break;

	case 'a':
	case 'd': {
		struct sockaddr_dl	*dlp;
		struct ether_addr	*ep;

		memset(&ifr, 0, sizeof(struct ifreq));
		dlp = (struct sockaddr_dl *)&ifr.ifr_addr;
		dlp->sdl_len = sizeof(struct sockaddr_dl);
		dlp->sdl_family = AF_LINK;
		dlp->sdl_index = 0;
		dlp->sdl_nlen = 0;
		dlp->sdl_alen = ETHER_ADDR_LEN;
		dlp->sdl_slen = 0;
		if (sscanf(line, "%s %s", str1, str2) != 2) {
			warnc(EINVAL, "sscanf");
			break;
		}
		ep = ether_aton(str2);
		if (ep == NULL) {
			warnc(EINVAL, "ether_aton");
			break;
		}
		strlcpy(ifr.ifr_name, str1, IF_NAMESIZE);
		memcpy(LLADDR(dlp), ep, ETHER_ADDR_LEN);
		if (ioctl(s, (*cmd == 'a') ? SIOCADDMULTI : SIOCDELMULTI,
		    &ifr) == -1)
			warn("ioctl SIOCADDMULTI/SIOCDELMULTI");
		else
			printf("ok\n");
		break;
	}

	case 'm':
		printf("warning: IFF_ALLMULTI cannot be set from userland "
		    "in FreeBSD; command ignored.\n");
		break;
	case 'p':
		if (sscanf(line, "%s %u", ifr.ifr_name, &f) != 2) {
			printf("-1\n");
			break;
		}
		if (ioctl(s, SIOCGIFFLAGS, &ifr) == -1) {
			warn("ioctl SIOCGIFFLAGS");
			break;
		}
		flags = (ifr.ifr_flags & 0xffff) | (ifr.ifr_flagshigh << 16);
		opt = IFF_PPROMISC;
		if (f == 0) {
			flags &= ~opt;
		} else {
			flags |= opt;
		}
		ifr.ifr_flags = flags & 0xffff;
		ifr.ifr_flagshigh = flags >> 16;
		if (ioctl(s, SIOCSIFFLAGS, &ifr) == -1)
			warn("ioctl SIOCGIFFLAGS");
		else
			printf( "changed to 0x%08x\n", flags );
		break;

#ifdef WITH_IGMPV3
	/*
	 * Set the socket to include or exclude filter mode, and
	 * add some sources to the filterlist, using the full-state,
	 * or advanced api.
	 */
	case 'i':
	case 'e':
		/* XXX: SIOCSIPMSFILTER will be made an internal API. */
		if ((sscanf(line, "%s %s %d", str1, str2, &n)) != 3) {
			printf("-1\n");
			break;
		}
		imsfp = (struct ip_msfilter *)filtbuf;
		if (((imsfp->imsf_multiaddr.s_addr = inet_addr(str1)) ==
		    INADDR_NONE) ||
		    ((imsfp->imsf_interface.s_addr = inet_addr(str2)) ==
		    INADDR_NONE) || (n > MAX_ADDRS)) {
			printf("-1\n");
			break;
		}
		imsfp->imsf_fmode = (*cmd == 'i') ? MCAST_INCLUDE :
		    MCAST_EXCLUDE;
		imsfp->imsf_numsrc = n;
		for (i = 0; i < n; i++) {
			fgets(str1, sizeof(str1), fp);
			if ((imsfp->imsf_slist[i].s_addr = inet_addr(str1)) ==
			    INADDR_NONE) {
				printf("-1\n");
				return;
			}
		}
		if (ioctl(s, SIOCSIPMSFILTER, imsfp) != 0)
			warn("setsockopt SIOCSIPMSFILTER");
		else
			printf("ok\n");
		break;
#endif /* WITH_IGMPV3 */

	/*
	 * Allow or block traffic from a source, using the
	 * delta based api.
	 * XXX: Currently we allow this to be used with the ASM-only
	 *      implementation of RFC3678 in FreeBSD 7. 
	 */
	case 't':
	case 'b':
		sscanf(line, "%s %s %s", str1, str2, str3);
		if (((imrs.imr_multiaddr.s_addr = inet_addr(str1)) ==
		    INADDR_NONE) ||
			((imrs.imr_interface.s_addr = inet_addr(str2)) ==
		    INADDR_NONE) ||
			((imrs.imr_sourceaddr.s_addr = inet_addr(str3)) ==
		    INADDR_NONE)) {
			printf("-1\n");
			break;
		}

#ifdef WITH_IGMPV3
		/* XXX: SIOCSIPMSFILTER will be made an internal API. */
		/* First determine out current filter mode. */
		imsfp = (struct ip_msfilter *)filtbuf;
		imsfp->imsf_multiaddr.s_addr = imrs.imr_multiaddr.s_addr;
		imsfp->imsf_interface.s_addr = imrs.imr_interface.s_addr;
		imsfp->imsf_numsrc = 5;
		if (ioctl(s, SIOCSIPMSFILTER, imsfp) != 0) {
			/* It's only okay for 't' to fail */
			if (*cmd != 't') {
				warn("ioctl SIOCSIPMSFILTER");
				break;
			} else {
				imsfp->imsf_fmode = MCAST_INCLUDE;
			}
		}
		if (imsfp->imsf_fmode == MCAST_EXCLUDE) {
			/* Any source */
			opt = (*cmd == 't') ? IP_UNBLOCK_SOURCE :
			    IP_BLOCK_SOURCE;
		} else {
			/* Controlled source */
			opt = (*cmd == 't') ? IP_ADD_SOURCE_MEMBERSHIP :
			    IP_DROP_SOURCE_MEMBERSHIP;
		}
#else /* !WITH_IGMPV3 */
		/*
		 * Don't look before we leap; we may only block or unblock
		 * sources on a socket in exclude mode.
		 */
		opt = (*cmd == 't') ? IP_UNBLOCK_SOURCE : IP_BLOCK_SOURCE;
#endif /* WITH_IGMPV3 */
		if (setsockopt(s, IPPROTO_IP, opt, &imrs, sizeof(imrs)) == -1)
			warn("ioctl IP_ADD_SOURCE_MEMBERSHIP/IP_DROP_SOURCE_MEMBERSHIP/IP_UNBLOCK_SOURCE/IP_BLOCK_SOURCE");
		else
			printf("ok\n");
		break;

#ifdef WITH_IGMPV3
	case 'g':
		/* XXX: SIOCSIPMSFILTER will be made an internal API. */
		if ((sscanf(line, "%s %s %d", str1, str2, &n)) != 3) {
			printf("-1\n");
			break;
		}
		imsfp = (struct ip_msfilter *)filtbuf;
		if (((imsfp->imsf_multiaddr.s_addr = inet_addr(str1)) ==
		    INADDR_NONE) ||
		    ((imsfp->imsf_interface.s_addr = inet_addr(str2)) ==
		    INADDR_NONE) || (n < 0 || n > MAX_ADDRS)) {
			printf("-1\n");
			break;
		}
		imsfp->imsf_numsrc = n;
		if (ioctl(s, SIOCSIPMSFILTER, imsfp) != 0) {
			warn("setsockopt SIOCSIPMSFILTER");
			break;
		}
		printf("%s\n", (imsfp->imsf_fmode == MCAST_INCLUDE) ?
		    "include" : "exclude");
		printf("%d\n", imsfp->imsf_numsrc);
		if (n >= imsfp->imsf_numsrc) {
			n = imsfp->imsf_numsrc;
			qsort(imsfp->imsf_slist, n, sizeof(struct in_addr),
			    &inaddr_cmp);
			for (i = 0; i < n; i++)
				printf("%s\n", inet_ntoa(imsfp->imsf_slist[i]));
		}
		break;
#endif	/* !WITH_IGMPV3 */

#ifndef WITH_IGMPV3
	case 'i':
	case 'e':
	case 'g':
		printf("warning: IGMPv3 is not supported by this version "
		    "of FreeBSD; command ignored.\n");
		break;
#endif	/* WITH_IGMPV3 */

	case '\n':
		break;
	default:
		printf("invalid command\n");
		break;
	}
}

static void
usage(void)
{

	printf("j g.g.g.g i.i.i.i          - join IP multicast group\n");
	printf("l g.g.g.g i.i.i.i          - leave IP multicast group\n");
	printf("a ifname e.e.e.e.e.e       - add ether multicast address\n");
	printf("d ifname e.e.e.e.e.e       - delete ether multicast address\n");
	printf("m ifname 1/0               - set/clear ether allmulti flag\n");
	printf("p ifname 1/0               - set/clear ether promisc flag\n");
#ifdef WITH_IGMPv3
	printf("i g.g.g.g i.i.i.i n        - set n include mode src filter\n");
	printf("e g.g.g.g i.i.i.i n        - set n exclude mode src filter\n");
#endif
	printf("t g.g.g.g i.i.i.i s.s.s.s  - allow traffic from src\n");
	printf("b g.g.g.g i.i.i.i s.s.s.s  - block traffic from src\n");
#ifdef WITH_IGMPV3
	printf("g g.g.g.g i.i.i.i n        - get and show n src filters\n");
#endif
	printf("f filename                 - read command(s) from file\n");
	printf("s seconds                  - sleep for some time\n");
	printf("q                          - quit\n");
}

#ifdef WITH_IGMPV3
static int
inaddr_cmp(const void *a, const void *b)
{
	return((int)((const struct in_addr *)a)->s_addr -
	    ((const struct in_addr *)b)->s_addr);
}
#endif
