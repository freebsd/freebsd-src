/*
 * Copyright (c) 1984, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Sun Microsystems, Inc.
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
static char const copyright[] =
"@(#) Copyright (c) 1984, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char const sccsid[] = "@(#)from: arp.c	8.2 (Berkeley) 1/2/94";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * arp - display, set, and delete arp table entries
 */


#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

void search(u_long addr, void (*action)(struct sockaddr_dl *sdl,
	struct sockaddr_inarp *sin, struct rt_msghdr *rtm));
void print_entry(struct sockaddr_dl *sdl,
	struct sockaddr_inarp *sin, struct rt_msghdr *rtm);
void nuke_entry(struct sockaddr_dl *sdl,
	struct sockaddr_inarp *sin, struct rt_msghdr *rtm);
int delete(char *host, char *info);
void ether_print(u_char *cp);
void usage(void);
int set(int argc, char **argv);
int get(char *host);
int file(char *name);
void getsocket(void);
int my_ether_aton(char *a, u_char *n);
int rtmsg(int cmd);
int get_ether_addr(u_int32_t ipaddr, u_char *hwaddr);

static int pid;
static int nflag;	/* no reverse dns lookups */
static int aflag;	/* do it for all entries */
static int s = -1;

/* which function we're supposed to do */
#define F_GET		1
#define F_SET		2
#define F_FILESET	3
#define F_REPLACE	4
#define F_DELETE	5

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define SETFUNC(f)	{ if (func) usage(); func = (f); }

int
main(int argc, char *argv[])
{
	int ch, func = 0;
	int rtn = 0;

	pid = getpid();
	while ((ch = getopt(argc, argv, "andfsS")) != -1)
		switch((char)ch) {
		case 'a':
			aflag = 1;
			break;
		case 'd':
			SETFUNC(F_DELETE);
			break;
		case 'n':
			nflag = 1;
			break;
		case 'S':
			SETFUNC(F_REPLACE);
			break;
		case 's':
			SETFUNC(F_SET);
			break;
		case 'f' :
			SETFUNC(F_FILESET);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!func)
		func = F_GET;
	switch (func) {
	case F_GET:
		if (aflag) {
			if (argc != 0)
				usage();
			search(0, print_entry);
		} else {
			if (argc != 1)
				usage();
			get(argv[0]);
		}
		break;
	case F_SET:
	case F_REPLACE:
		if (argc < 2 || argc > 6)
			usage();
		if (func == F_REPLACE)
			(void) delete(argv[0], NULL);
		rtn = set(argc, argv) ? 1 : 0;
		break;
	case F_DELETE:
		if (aflag) {
			if (argc != 0)
				usage();
			search(0, nuke_entry);
		} else {
			if (argc < 1 || argc > 2)
				usage();
			rtn = delete(argv[0], argv[1]);
		}
		break;
	case F_FILESET:
		if (argc != 1)
			usage();
		rtn = file(argv[0]);
		break;
	}

	return(rtn);
}

/*
 * Process a file to set standard arp entries
 */
int
file(char *name)
{
	FILE *fp;
	int i, retval;
	char line[100], arg[5][50], *args[5];

	if ((fp = fopen(name, "r")) == NULL)
		errx(1, "cannot open %s", name);
	args[0] = &arg[0][0];
	args[1] = &arg[1][0];
	args[2] = &arg[2][0];
	args[3] = &arg[3][0];
	args[4] = &arg[4][0];
	retval = 0;
	while(fgets(line, 100, fp) != NULL) {
		i = sscanf(line, "%49s %49s %49s %49s %49s", arg[0], arg[1],
		    arg[2], arg[3], arg[4]);
		if (i < 2) {
			warnx("bad line: %s", line);
			retval = 1;
			continue;
		}
		if (set(i, args))
			retval = 1;
	}
	fclose(fp);
	return (retval);
}

void
getsocket(void)
{
	if (s < 0) {
		s = socket(PF_ROUTE, SOCK_RAW, 0);
		if (s < 0)
			err(1, "socket");
	}
}

struct	sockaddr_in so_mask = {8, 0, 0, { 0xffffffff}};
struct	sockaddr_inarp blank_sin = {sizeof(blank_sin), AF_INET }, sin_m;
struct	sockaddr_dl blank_sdl = {sizeof(blank_sdl), AF_LINK }, sdl_m;
int	expire_time, flags, doing_proxy, proxy_only, found_entry;
struct	{
	struct	rt_msghdr m_rtm;
	char	m_space[512];
}	m_rtmsg;

/*
 * Set an individual arp entry
 */
int
set(int argc, char **argv)
{
	struct hostent *hp;
	register struct sockaddr_inarp *sin = &sin_m;
	register struct sockaddr_dl *sdl;
	register struct rt_msghdr *rtm = &(m_rtmsg.m_rtm);
	u_char *ea;
	char *host = argv[0], *eaddr = argv[1];

	getsocket();
	argc -= 2;
	argv += 2;
	sdl_m = blank_sdl;
	sin_m = blank_sin;
	sin->sin_addr.s_addr = inet_addr(host);
	if (sin->sin_addr.s_addr == INADDR_NONE) {
		if (!(hp = gethostbyname(host))) {
			warnx("%s: %s", host, hstrerror(h_errno));
			return (1);
		}
		bcopy((char *)hp->h_addr, (char *)&sin->sin_addr,
		    sizeof sin->sin_addr);
	}
	doing_proxy = flags = proxy_only = expire_time = 0;
	while (argc-- > 0) {
		if (strncmp(argv[0], "temp", 4) == 0) {
			struct timeval tv;
			gettimeofday(&tv, 0);
			expire_time = tv.tv_sec + 20 * 60;
		}
		else if (strncmp(argv[0], "pub", 3) == 0) {
			flags |= RTF_ANNOUNCE;
			doing_proxy = 1;
			if (argc && strncmp(argv[1], "only", 3) == 0) {
				proxy_only = 1;
				sin_m.sin_other = SIN_PROXY;
				argc--; argv++;
			}
		} else if (strncmp(argv[0], "trail", 5) == 0) {
			printf("%s: Sending trailers is no longer supported\n",
				host);
		}
		argv++;
	}
	ea = (u_char *)LLADDR(&sdl_m);
	if (doing_proxy && !strcmp(eaddr, "auto")) {
		if (!get_ether_addr(sin->sin_addr.s_addr, ea)) {
			printf("no interface found for %s\n",
			       inet_ntoa(sin->sin_addr));
			return (1);
		}
		sdl_m.sdl_alen = 6;
	} else {
		if (my_ether_aton(eaddr, ea) == 0)
			sdl_m.sdl_alen = 6;
	}
tryagain:
	if (rtmsg(RTM_GET) < 0) {
		warn("%s", host);
		return (1);
	}
	sin = (struct sockaddr_inarp *)(rtm + 1);
	sdl = (struct sockaddr_dl *)(ROUNDUP(sin->sin_len) + (char *)sin);
	if (sin->sin_addr.s_addr == sin_m.sin_addr.s_addr) {
		if (sdl->sdl_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLINFO) &&
		    !(rtm->rtm_flags & RTF_GATEWAY)) switch (sdl->sdl_type) {
		case IFT_ETHER: case IFT_FDDI: case IFT_ISO88023:
		case IFT_ISO88024: case IFT_ISO88025: case IFT_L2VLAN:
			goto overwrite;
		}
		if (doing_proxy == 0) {
			printf("set: can only proxy for %s\n", host);
			return (1);
		}
		if (sin_m.sin_other & SIN_PROXY) {
			printf("set: proxy entry exists for non 802 device\n");
			return(1);
		}
		sin_m.sin_other = SIN_PROXY;
		proxy_only = 1;
		goto tryagain;
	}
overwrite:
	if (sdl->sdl_family != AF_LINK) {
		printf("cannot intuit interface index and type for %s\n", host);
		return (1);
	}
	sdl_m.sdl_type = sdl->sdl_type;
	sdl_m.sdl_index = sdl->sdl_index;
	return (rtmsg(RTM_ADD));
}

/*
 * Display an individual arp entry
 */
int
get(char *host)
{
	struct hostent *hp;
	struct sockaddr_inarp *sin = &sin_m;

	sin_m = blank_sin;
	sin->sin_addr.s_addr = inet_addr(host);
	if (sin->sin_addr.s_addr == INADDR_NONE) {
		if (!(hp = gethostbyname(host)))
			errx(1, "%s: %s", host, hstrerror(h_errno));
		bcopy((char *)hp->h_addr, (char *)&sin->sin_addr,
		    sizeof sin->sin_addr);
	}
	search(sin->sin_addr.s_addr, print_entry);
	if (found_entry == 0) {
		printf("%s (%s) -- no entry\n",
		    host, inet_ntoa(sin->sin_addr));
		return(1);
	}
	return(0);
}

/*
 * Delete an arp entry
 */
int
delete(char *host, char *info)
{
	struct hostent *hp;
	register struct sockaddr_inarp *sin = &sin_m;
	register struct rt_msghdr *rtm = &m_rtmsg.m_rtm;
	struct sockaddr_dl *sdl;

	getsocket();
	sin_m = blank_sin;
	if (info) {
		if (strncmp(info, "pub", 3) == 0)
			sin_m.sin_other = SIN_PROXY;
		else
			usage();
	}
	sin->sin_addr.s_addr = inet_addr(host);
	if (sin->sin_addr.s_addr == INADDR_NONE) {
		if (!(hp = gethostbyname(host))) {
			warnx("%s: %s", host, hstrerror(h_errno));
			return (1);
		}
		bcopy((char *)hp->h_addr, (char *)&sin->sin_addr,
		    sizeof sin->sin_addr);
	}
tryagain:
	if (rtmsg(RTM_GET) < 0) {
		warn("%s", host);
		return (1);
	}
	sin = (struct sockaddr_inarp *)(rtm + 1);
	sdl = (struct sockaddr_dl *)(ROUNDUP(sin->sin_len) + (char *)sin);
	if (sin->sin_addr.s_addr == sin_m.sin_addr.s_addr) {
		if (sdl->sdl_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLINFO) &&
		    !(rtm->rtm_flags & RTF_GATEWAY)) switch (sdl->sdl_type) {
		case IFT_ETHER: case IFT_FDDI: case IFT_ISO88023:
		case IFT_ISO88024: case IFT_ISO88025: case IFT_L2VLAN:
			goto delete;
		}
	}
	if (sin_m.sin_other & SIN_PROXY) {
		fprintf(stderr, "delete: can't locate %s\n",host);
		return (1);
	} else {
		sin_m.sin_other = SIN_PROXY;
		goto tryagain;
	}
delete:
	if (sdl->sdl_family != AF_LINK) {
		printf("cannot locate %s\n", host);
		return (1);
	}
	if (rtmsg(RTM_DELETE) == 0) {
		printf("%s (%s) deleted\n", host, inet_ntoa(sin->sin_addr));
		return (0);
	}
	return (1);
}

/*
 * Search the arp table and do some action on matching entries
 */
void
search(u_long addr, void (*action)(struct sockaddr_dl *sdl,
	struct sockaddr_inarp *sin, struct rt_msghdr *rtm))
{
	int mib[6];
	size_t needed;
	char *lim, *buf, *next;
	struct rt_msghdr *rtm;
	struct sockaddr_inarp *sin;
	struct sockaddr_dl *sdl;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_LLINFO;
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		errx(1, "route-sysctl-estimate");
	if ((buf = malloc(needed)) == NULL)
		errx(1, "malloc");
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		errx(1, "actual retrieval of routing table");
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		sin = (struct sockaddr_inarp *)(rtm + 1);
		(char *)sdl = (char *)sin + ROUNDUP(sin->sin_len);
		if (addr) {
			if (addr != sin->sin_addr.s_addr)
				continue;
			found_entry = 1;
		}
		(*action)(sdl, sin, rtm);
	}
	free(buf);
}

/*
 * Display an arp entry
 */
void
print_entry(struct sockaddr_dl *sdl,
	struct sockaddr_inarp *sin, struct rt_msghdr *rtm)
{
	const char *host;
	struct hostent *hp;
	int seg;

	if (nflag == 0)
		hp = gethostbyaddr((caddr_t)&(sin->sin_addr),
		    sizeof sin->sin_addr, AF_INET);
	else
		hp = 0;
	if (hp)
		host = hp->h_name;
	else {
		host = "?";
		if (h_errno == TRY_AGAIN)
			nflag = 1;
	}
	printf("%s (%s) at ", host, inet_ntoa(sin->sin_addr));
	if (sdl->sdl_alen)
		ether_print(LLADDR(sdl));
	else
		printf("(incomplete)");
	if (rtm->rtm_rmx.rmx_expire == 0)
		printf(" permanent");
	if (sin->sin_other & SIN_PROXY)
		printf(" published (proxy only)");
	if (rtm->rtm_addrs & RTA_NETMASK) {
		sin = (struct sockaddr_inarp *)
			(ROUNDUP(sdl->sdl_len) + (char *)sdl);
		if (sin->sin_addr.s_addr == 0xffffffff)
			printf(" published");
		if (sin->sin_len != 8)
			printf("(weird)");
	}
        switch(sdl->sdl_type) {
            case IFT_ETHER:
                printf(" [ethernet]");
                break;
            case IFT_ISO88025:
                printf(" [token-ring]");
                break;
	    case IFT_L2VLAN:
		printf(" [vlan]");
		break;
            default:
        }
	if (sdl->sdl_rcf != NULL) {
		printf(" rt=%x", ntohs(sdl->sdl_rcf));
		for (seg = 0; seg < ((((ntohs(sdl->sdl_rcf) & 0x1f00) >> 8) - 2 ) / 2); seg++) 
			printf(":%x", ntohs(sdl->sdl_route[seg]));
	}
		
	printf("\n");

}

/*
 * Nuke an arp entry
 */
void
nuke_entry(struct sockaddr_dl *sdl,
	struct sockaddr_inarp *sin, struct rt_msghdr *rtm)
{
	char ip[20];

	snprintf(ip, sizeof(ip), "%s", inet_ntoa(sin->sin_addr));
	delete(ip, NULL);
}

void
ether_print(u_char *cp)
{
	printf("%x:%x:%x:%x:%x:%x", cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
}

int
my_ether_aton(char *a, u_char *n)
{
	int i, o[6];

	i = sscanf(a, "%x:%x:%x:%x:%x:%x", &o[0], &o[1], &o[2],
					   &o[3], &o[4], &o[5]);
	if (i != 6) {
		warnx("invalid Ethernet address '%s'", a);
		return (1);
	}
	for (i=0; i<6; i++)
		n[i] = o[i];
	return (0);
}

void
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
		"usage: arp [-n] hostname",
		"       arp [-n] -a",
		"       arp -d hostname [pub]",
		"       arp -d -a",
		"       arp -s hostname ether_addr [temp] [pub]",
		"       arp -S hostname ether_addr [temp] [pub]",
		"       arp -f filename");
	exit(1);
}

int
rtmsg(int cmd)
{
	static int seq;
	int rlen;
	register struct rt_msghdr *rtm = &m_rtmsg.m_rtm;
	register char *cp = m_rtmsg.m_space;
	register int l;

	errno = 0;
	if (cmd == RTM_DELETE)
		goto doit;
	bzero((char *)&m_rtmsg, sizeof(m_rtmsg));
	rtm->rtm_flags = flags;
	rtm->rtm_version = RTM_VERSION;

	switch (cmd) {
	default:
		errx(1, "internal wrong cmd");
	case RTM_ADD:
		rtm->rtm_addrs |= RTA_GATEWAY;
		rtm->rtm_rmx.rmx_expire = expire_time;
		rtm->rtm_inits = RTV_EXPIRE;
		rtm->rtm_flags |= (RTF_HOST | RTF_STATIC);
		sin_m.sin_other = 0;
		if (doing_proxy) {
			if (proxy_only)
				sin_m.sin_other = SIN_PROXY;
			else {
				rtm->rtm_addrs |= RTA_NETMASK;
				rtm->rtm_flags &= ~RTF_HOST;
			}
		}
		/* FALLTHROUGH */
	case RTM_GET:
		rtm->rtm_addrs |= RTA_DST;
	}
#define NEXTADDR(w, s) \
	if (rtm->rtm_addrs & (w)) { \
		bcopy((char *)&s, cp, sizeof(s)); cp += ROUNDUP(sizeof(s));}

	NEXTADDR(RTA_DST, sin_m);
	NEXTADDR(RTA_GATEWAY, sdl_m);
	NEXTADDR(RTA_NETMASK, so_mask);

	rtm->rtm_msglen = cp - (char *)&m_rtmsg;
doit:
	l = rtm->rtm_msglen;
	rtm->rtm_seq = ++seq;
	rtm->rtm_type = cmd;
	if ((rlen = write(s, (char *)&m_rtmsg, l)) < 0) {
		if (errno != ESRCH || cmd != RTM_DELETE) {
			warn("writing to routing socket");
			return (-1);
		}
	}
	do {
		l = read(s, (char *)&m_rtmsg, sizeof(m_rtmsg));
	} while (l > 0 && (rtm->rtm_seq != seq || rtm->rtm_pid != pid));
	if (l < 0)
		warn("read from routing socket");
	return (0);
}

/*
 * get_ether_addr - get the hardware address of an interface on the
 * the same subnet as ipaddr.
 */
#define MAX_IFS		32

int
get_ether_addr(u_int32_t ipaddr, u_char *hwaddr)
{
	struct ifreq *ifr, *ifend, *ifp;
	u_int32_t ina, mask;
	struct sockaddr_dl *dla;
	struct ifreq ifreq;
	struct ifconf ifc;
	struct ifreq ifs[MAX_IFS];
	int s;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		err(1, "socket");

	ifc.ifc_len = sizeof(ifs);
	ifc.ifc_req = ifs;
	if (ioctl(s, SIOCGIFCONF, &ifc) < 0) {
		warnx("ioctl(SIOCGIFCONF)");
		close(s);
		return 0;
	}

	/*
	* Scan through looking for an interface with an Internet
	* address on the same subnet as `ipaddr'.
	*/
	ifend = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
	for (ifr = ifc.ifc_req; ifr < ifend; ) {
		if (ifr->ifr_addr.sa_family == AF_INET) {
			ina = ((struct sockaddr_in *) 
				&ifr->ifr_addr)->sin_addr.s_addr;
			strncpy(ifreq.ifr_name, ifr->ifr_name, 
				sizeof(ifreq.ifr_name));
			/*
			 * Check that the interface is up,
			 * and not point-to-point or loopback.
			 */
			if (ioctl(s, SIOCGIFFLAGS, &ifreq) < 0)
				continue;
			if ((ifreq.ifr_flags &
			     (IFF_UP|IFF_BROADCAST|IFF_POINTOPOINT|
					IFF_LOOPBACK|IFF_NOARP))
			     != (IFF_UP|IFF_BROADCAST))
				goto nextif;
			/*
			 * Get its netmask and check that it's on 
			 * the right subnet.
			 */
			if (ioctl(s, SIOCGIFNETMASK, &ifreq) < 0)
				continue;
			mask = ((struct sockaddr_in *)
				&ifreq.ifr_addr)->sin_addr.s_addr;
			if ((ipaddr & mask) != (ina & mask))
				goto nextif;
			break;
		}
nextif:
		ifr = (struct ifreq *) ((char *)&ifr->ifr_addr
		    + MAX(ifr->ifr_addr.sa_len, sizeof(ifr->ifr_addr)));
	}

	if (ifr >= ifend) {
		close(s);
		return 0;
	}

	/*
	* Now scan through again looking for a link-level address
	* for this interface.
	*/
	ifp = ifr;
	for (ifr = ifc.ifc_req; ifr < ifend; ) {
		if (strcmp(ifp->ifr_name, ifr->ifr_name) == 0
		    && ifr->ifr_addr.sa_family == AF_LINK) {
			/*
			 * Found the link-level address - copy it out
			 */
		 	dla = (struct sockaddr_dl *) &ifr->ifr_addr;
			memcpy(hwaddr,  LLADDR(dla), dla->sdl_alen);
			close (s);
			printf("using interface %s for proxy with address ",
				ifp->ifr_name);
			ether_print(hwaddr);
			printf("\n");
			return dla->sdl_alen;
		}
		ifr = (struct ifreq *) ((char *)&ifr->ifr_addr
		    + MAX(ifr->ifr_addr.sa_len, sizeof(ifr->ifr_addr)));
	}
	return 0;
}
