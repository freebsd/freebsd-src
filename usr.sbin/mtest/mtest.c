/*-
 * Copyright (c) 2007-2009 Bruce Simpson.
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

static void	process_file(char *, int);
static void	process_cmd(char*, int, FILE *fp);
static void	usage(void);

#define	MAX_ADDRS	20
#define	STR_SIZE	20
#define	LINE_LENGTH	80

static int
inaddr_cmp(const void *a, const void *b)
{
	return ((int)((const struct in_addr *)a)->s_addr -
	    ((const struct in_addr *)b)->s_addr);
}

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
	struct in_addr		 sources[MAX_ADDRS];
	struct ifreq		 ifr;
	struct ip_mreq		 imr;
	struct ip_mreq_source	 imrs;
	char			*line;
	uint32_t		 fmode;
	int			 i, n, opt, f, flags;

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
		str3[0] = '\0';
		sscanf(line, "%s %s %s", str1, str2, str3);
		if ((imrs.imr_sourceaddr.s_addr = inet_addr(str3)) !=
		    INADDR_NONE) {
			/*
			 * inclusive mode join with source, possibly
			 * on existing membership.
			 */
			if (((imrs.imr_multiaddr.s_addr = inet_addr(str1)) ==
			    INADDR_NONE) ||
			    ((imrs.imr_interface.s_addr = inet_addr(str2)) ==
			    INADDR_NONE)) {
				printf("-1\n");
				break;
			}
			opt = (*cmd == 'j') ? IP_ADD_SOURCE_MEMBERSHIP :
			    IP_DROP_SOURCE_MEMBERSHIP;
			if (setsockopt( s, IPPROTO_IP, opt, &imrs,
			    sizeof(imrs)) != 0) {
				warn("setsockopt %s", (*cmd == 'j') ?
				    "IP_ADD_SOURCE_MEMBERSHIP" :
				    "IP_DROP_SOURCE_MEMBERSHIP");
			} else {
				printf("ok\n");
			}
		} else {
			/* exclusive mode join w/o source. */
			if (((imr.imr_multiaddr.s_addr = inet_addr(str1)) ==
			    INADDR_NONE) ||
			    ((imr.imr_interface.s_addr = inet_addr(str2)) ==
			    INADDR_NONE)) {
				printf("-1\n");
				break;
			}
			opt = (*cmd == 'j') ? IP_ADD_MEMBERSHIP :
			    IP_DROP_MEMBERSHIP;
			if (setsockopt( s, IPPROTO_IP, opt, &imr,
			    sizeof(imr)) != 0) {
				warn("setsockopt %s", (*cmd == 'j') ?
				    "IP_ADD_MEMBERSHIP" :
				    "IP_DROP_MEMBERSHIP");
			} else {
				printf("ok\n");
			}
		}
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

	/*
	 * Set the socket to include or exclude filter mode, and
	 * add some sources to the filterlist, using the full-state,
	 * or advanced api.
	 */
	case 'i':
	case 'e':
		n = 0;
		fmode = (*cmd == 'i') ? MCAST_INCLUDE : MCAST_EXCLUDE;
		if ((sscanf(line, "%s %s %d", str1, str2, &n)) != 3) {
			printf("-1\n");
			break;
		}
		/* recycle imrs struct for convenience */
		if (((imrs.imr_multiaddr.s_addr = inet_addr(str1)) ==
		    INADDR_NONE) ||
		    ((imrs.imr_interface.s_addr = inet_addr(str2)) ==
		    INADDR_NONE) || (n < 0 || n > MAX_ADDRS)) {
			printf("-1\n");
			break;
		}
		for (i = 0; i < n; i++) {
			fgets(str1, sizeof(str1), fp);
			if ((sources[i].s_addr = inet_addr(str1)) ==
			    INADDR_NONE) {
				printf("-1\n");
				return;
			}
		}
		if (setipv4sourcefilter(s, imrs.imr_interface,
		    imrs.imr_multiaddr, fmode, n, sources) != 0)
			warn("getipv4sourcefilter");
		else
			printf("ok\n");
		break;

	/*
	 * Allow or block traffic from a source, using the
	 * delta based api.
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
		/* First determine our current filter mode. */
		n = 0;
		if (getipv4sourcefilter(s, imrs.imr_interface,
		    imrs.imr_multiaddr, &fmode, &n, NULL) != 0) {
			warn("getipv4sourcefilter");
			break;
		}
		if (fmode == MCAST_EXCLUDE) {
			/* Any source */
			opt = (*cmd == 't') ? IP_UNBLOCK_SOURCE :
			    IP_BLOCK_SOURCE;
		} else {
			/* Controlled source */
			opt = (*cmd == 't') ? IP_ADD_SOURCE_MEMBERSHIP :
			    IP_DROP_SOURCE_MEMBERSHIP;
		}
		if (setsockopt(s, IPPROTO_IP, opt, &imrs, sizeof(imrs)) == -1)
			warn("ioctl IP_ADD_SOURCE_MEMBERSHIP/IP_DROP_SOURCE_MEMBERSHIP/IP_UNBLOCK_SOURCE/IP_BLOCK_SOURCE");
		else
			printf("ok\n");
		break;

	case 'g':
		if ((sscanf(line, "%s %s %d", str1, str2, &n)) != 3) {
			printf("-1\n");
			break;
		}
		/* recycle imrs struct for convenience */
		if (((imrs.imr_multiaddr.s_addr = inet_addr(str1)) ==
		    INADDR_NONE) ||
		    ((imrs.imr_interface.s_addr = inet_addr(str2)) ==
		    INADDR_NONE) || (n < 0 || n > MAX_ADDRS)) {
			printf("-1\n");
			break;
		}
		if (getipv4sourcefilter(s, imrs.imr_interface,
		    imrs.imr_multiaddr, &fmode, &n, sources) != 0) {
			warn("getipv4sourcefilter");
			break;
		}
		printf("%s\n", (fmode == MCAST_INCLUDE) ? "include" :
		    "exclude");
		printf("%d\n", n);
		qsort(sources, n, sizeof(struct in_addr), &inaddr_cmp);
		for (i = 0; i < n; i++)
			printf("%s\n", inet_ntoa(sources[i]));
		break;

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

	printf("j g.g.g.g i.i.i.i [s.s.s.s] - join IP multicast group\n");
	printf("l g.g.g.g i.i.i.i [s.s.s.s] - leave IP multicast group\n");
	printf("a ifname e.e.e.e.e.e       - add ether multicast address\n");
	printf("d ifname e.e.e.e.e.e       - delete ether multicast address\n");
	printf("m ifname 1/0               - set/clear ether allmulti flag\n");
	printf("p ifname 1/0               - set/clear ether promisc flag\n");
	printf("i g.g.g.g i.i.i.i n        - set n include mode src filter\n");
	printf("e g.g.g.g i.i.i.i n        - set n exclude mode src filter\n");
	printf("t g.g.g.g i.i.i.i s.s.s.s  - allow traffic from src\n");
	printf("b g.g.g.g i.i.i.i s.s.s.s  - block traffic from src\n");
	printf("g g.g.g.g i.i.i.i n        - get and show n src filters\n");
	printf("f filename                 - read command(s) from file\n");
	printf("s seconds                  - sleep for some time\n");
	printf("q                          - quit\n");
}
