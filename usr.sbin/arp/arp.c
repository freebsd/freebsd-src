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

#if 0
#ifndef lint
static char const copyright[] =
"@(#) Copyright (c) 1984, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char const sccsid[] = "@(#)from: arp.c	8.2 (Berkeley) 1/2/94";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <net/iso88025.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

typedef void (action_fn)(struct sockaddr_dl *sdl,
	struct sockaddr_inarp *s_in, struct rt_msghdr *rtm);

static int search(u_long addr, action_fn *action);
static action_fn print_entry;
static action_fn nuke_entry;

static int delete(char *host, int do_proxy);
static void usage(void);
static int set(int argc, char **argv);
static int get(char *host);
static int file(char *name);
static struct rt_msghdr *rtmsg(int cmd,
    struct sockaddr_inarp *dst, struct sockaddr_dl *sdl);
static int get_ether_addr(in_addr_t ipaddr, struct ether_addr *hwaddr);
static struct sockaddr_inarp *getaddr(char *host);
static int valid_type(int type);

static int nflag;	/* no reverse dns lookups */
static char *rifname;

static int	expire_time, flags, doing_proxy, proxy_only;

/* which function we're supposed to do */
#define F_GET		1
#define F_SET		2
#define F_FILESET	3
#define F_REPLACE	4
#define F_DELETE	5

#define SETFUNC(f)	{ if (func) usage(); func = (f); }

int
main(int argc, char *argv[])
{
	int ch, func = 0;
	int rtn = 0;
	int aflag = 0;	/* do it for all entries */

	while ((ch = getopt(argc, argv, "andfsSi:")) != -1)
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
		case 'i':
			rifname = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!func)
		func = F_GET;
	if (rifname) {
		if (func != F_GET && !(func == F_DELETE && aflag))
			errx(1, "-i not applicable to this operation");
		if (if_nametoindex(rifname) == 0) {
			if (errno == ENXIO)
				errx(1, "interface %s does not exist", rifname);
			else
				err(1, "if_nametoindex(%s)", rifname);
		}
	}
	switch (func) {
	case F_GET:
		if (aflag) {
			if (argc != 0)
				usage();
			search(0, print_entry);
		} else {
			if (argc != 1)
				usage();
			rtn = get(argv[0]);
		}
		break;
	case F_SET:
	case F_REPLACE:
		if (argc < 2 || argc > 6)
			usage();
		if (func == F_REPLACE)
			(void)delete(argv[0], 0);
		rtn = set(argc, argv) ? 1 : 0;
		break;
	case F_DELETE:
		if (aflag) {
			if (argc != 0)
				usage();
			search(0, nuke_entry);
		} else {
			if (argc == 2 && strncmp(argv[1], "pub", 3) == 0)
				ch = SIN_PROXY;
			else if (argc == 1)
				ch = 0;
			else
				usage();
			rtn = delete(argv[0], ch);
		}
		break;
	case F_FILESET:
		if (argc != 1)
			usage();
		rtn = file(argv[0]);
		break;
	}

	return (rtn);
}

/*
 * Process a file to set standard arp entries
 */
static int
file(char *name)
{
	FILE *fp;
	int i, retval;
	char line[100], arg[5][50], *args[5], *p;

	if ((fp = fopen(name, "r")) == NULL)
		err(1, "cannot open %s", name);
	args[0] = &arg[0][0];
	args[1] = &arg[1][0];
	args[2] = &arg[2][0];
	args[3] = &arg[3][0];
	args[4] = &arg[4][0];
	retval = 0;
	while(fgets(line, 100, fp) != NULL) {
		if ((p = strchr(line, '#')) != NULL)
			*p = '\0';
		for (p = line; isblank(*p); p++);
		if (*p == '\n' || *p == '\0')
			continue;
		i = sscanf(p, "%49s %49s %49s %49s %49s", arg[0], arg[1],
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

/*
 * Given a hostname, fills up a (static) struct sockaddr_inarp with
 * the address of the host and returns a pointer to the
 * structure.
 */
static struct sockaddr_inarp *
getaddr(char *host)
{
	struct hostent *hp;
	static struct sockaddr_inarp reply;

	bzero(&reply, sizeof(reply));
	reply.sin_len = sizeof(reply);
	reply.sin_family = AF_INET;
	reply.sin_addr.s_addr = inet_addr(host);
	if (reply.sin_addr.s_addr == INADDR_NONE) {
		if (!(hp = gethostbyname(host))) {
			warnx("%s: %s", host, hstrerror(h_errno));
			return (NULL);
		}
		bcopy((char *)hp->h_addr, (char *)&reply.sin_addr,
			sizeof reply.sin_addr);
	}
	return (&reply);
}

/*
 * Returns true if the type is a valid one for ARP.
 */
static int
valid_type(int type)
{

	switch (type) {
	case IFT_ETHER:
	case IFT_FDDI:
	case IFT_ISO88023:
	case IFT_ISO88024:
	case IFT_ISO88025:
	case IFT_L2VLAN:
	case IFT_BRIDGE:
		return (1);
	default:
		return (0);
	}
}

/*
 * Set an individual arp entry
 */
static int
set(int argc, char **argv)
{
	struct sockaddr_inarp *addr;
	struct sockaddr_inarp *dst;	/* what are we looking for */
	struct sockaddr_dl *sdl;
	struct rt_msghdr *rtm;
	struct ether_addr *ea;
	char *host = argv[0], *eaddr = argv[1];
	struct sockaddr_dl sdl_m;

	argc -= 2;
	argv += 2;

	bzero(&sdl_m, sizeof(sdl_m));
	sdl_m.sdl_len = sizeof(sdl_m);
	sdl_m.sdl_family = AF_LINK;

	dst = getaddr(host);
	if (dst == NULL)
		return (1);
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
				dst->sin_other = SIN_PROXY;
				argc--; argv++;
			}
		} else if (strncmp(argv[0], "trail", 5) == 0) {
			/* XXX deprecated and undocumented feature */
			printf("%s: Sending trailers is no longer supported\n",
				host);
		}
		argv++;
	}
	ea = (struct ether_addr *)LLADDR(&sdl_m);
	if (doing_proxy && !strcmp(eaddr, "auto")) {
		if (!get_ether_addr(dst->sin_addr.s_addr, ea)) {
			printf("no interface found for %s\n",
			       inet_ntoa(dst->sin_addr));
			return (1);
		}
		sdl_m.sdl_alen = ETHER_ADDR_LEN;
	} else {
		struct ether_addr *ea1 = ether_aton(eaddr);

		if (ea1 == NULL)
			warnx("invalid Ethernet address '%s'", eaddr);
		else {
			*ea = *ea1;
			sdl_m.sdl_alen = ETHER_ADDR_LEN;
		}
	}
	for (;;) {	/* try at most twice */
		rtm = rtmsg(RTM_GET, dst, &sdl_m);
		if (rtm == NULL) {
			warn("%s", host);
			return (1);
		}
		addr = (struct sockaddr_inarp *)(rtm + 1);
		sdl = (struct sockaddr_dl *)(SA_SIZE(addr) + (char *)addr);
		if (addr->sin_addr.s_addr != dst->sin_addr.s_addr)	
			break;
		if (sdl->sdl_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLINFO) &&
		    !(rtm->rtm_flags & RTF_GATEWAY) &&
		    valid_type(sdl->sdl_type) )
			break;
		if (doing_proxy == 0) {
			printf("set: can only proxy for %s\n", host);
			return (1);
		}
		if (dst->sin_other & SIN_PROXY) {
			printf("set: proxy entry exists for non 802 device\n");
			return (1);
		}
		dst->sin_other = SIN_PROXY;
		proxy_only = 1;
	}

	if (sdl->sdl_family != AF_LINK) {
		printf("cannot intuit interface index and type for %s\n", host);
		return (1);
	}
	sdl_m.sdl_type = sdl->sdl_type;
	sdl_m.sdl_index = sdl->sdl_index;
	return (rtmsg(RTM_ADD, dst, &sdl_m) == NULL);
}

/*
 * Display an individual arp entry
 */
static int
get(char *host)
{
	struct sockaddr_inarp *addr;

	addr = getaddr(host);
	if (addr == NULL)
		return (1);
	if (0 == search(addr->sin_addr.s_addr, print_entry)) {
		printf("%s (%s) -- no entry",
		    host, inet_ntoa(addr->sin_addr));
		if (rifname)
			printf(" on %s", rifname);
		printf("\n");
		return (1);
	}
	return (0);
}

/*
 * Delete an arp entry
 */
static int
delete(char *host, int do_proxy)
{
	struct sockaddr_inarp *addr, *dst;
	struct rt_msghdr *rtm;
	struct sockaddr_dl *sdl;

	dst = getaddr(host);
	if (dst == NULL)
		return (1);
	dst->sin_other = do_proxy;
	for (;;) {	/* try twice */
		rtm = rtmsg(RTM_GET, dst, NULL);
		if (rtm == NULL) {
			warn("%s", host);
			return (1);
		}
		addr = (struct sockaddr_inarp *)(rtm + 1);
		sdl = (struct sockaddr_dl *)(SA_SIZE(addr) + (char *)addr);
		if (addr->sin_addr.s_addr == dst->sin_addr.s_addr &&
		    sdl->sdl_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLINFO) &&
		    !(rtm->rtm_flags & RTF_GATEWAY) &&
		    valid_type(sdl->sdl_type) )
			break;	/* found it */
		if (dst->sin_other & SIN_PROXY) {
			fprintf(stderr, "delete: cannot locate %s\n",host);
			return (1);
		}
		dst->sin_other = SIN_PROXY;
	}
	if (rtmsg(RTM_DELETE, dst, NULL) != NULL) {
		printf("%s (%s) deleted\n", host, inet_ntoa(addr->sin_addr));
		return (0);
	}
	return (1);
}

/*
 * Search the arp table and do some action on matching entries
 */
static int
search(u_long addr, action_fn *action)
{
	int mib[6];
	size_t needed;
	char *lim, *buf, *newbuf, *next;
	struct rt_msghdr *rtm;
	struct sockaddr_inarp *sin2;
	struct sockaddr_dl *sdl;
	char ifname[IF_NAMESIZE];
	int st, found_entry = 0;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_LLINFO;
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		err(1, "route-sysctl-estimate");
	if (needed == 0)	/* empty table */
		return 0;
	buf = NULL;
	for (;;) {
		newbuf = realloc(buf, needed);
		if (newbuf == NULL) {
			if (buf != NULL)
				free(buf);
			errx(1, "could not reallocate memory");
		}
		buf = newbuf;
		st = sysctl(mib, 6, buf, &needed, NULL, 0);
		if (st == 0 || errno != ENOMEM)
			break;
		needed += needed / 8;
	}
	if (st == -1)
		err(1, "actual retrieval of routing table");
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		sin2 = (struct sockaddr_inarp *)(rtm + 1);
		sdl = (struct sockaddr_dl *)((char *)sin2 + SA_SIZE(sin2));
		if (rifname && if_indextoname(sdl->sdl_index, ifname) &&
		    strcmp(ifname, rifname))
			continue;
		if (addr) {
			if (addr != sin2->sin_addr.s_addr)
				continue;
			found_entry = 1;
		}
		(*action)(sdl, sin2, rtm);
	}
	free(buf);
	return (found_entry);
}

/*
 * Display an arp entry
 */
static void
print_entry(struct sockaddr_dl *sdl,
	struct sockaddr_inarp *addr, struct rt_msghdr *rtm)
{
	const char *host;
	struct hostent *hp;
	struct iso88025_sockaddr_dl_data *trld;
	char ifname[IF_NAMESIZE];
	int seg;

	if (nflag == 0)
		hp = gethostbyaddr((caddr_t)&(addr->sin_addr),
		    sizeof addr->sin_addr, AF_INET);
	else
		hp = 0;
	if (hp)
		host = hp->h_name;
	else {
		host = "?";
		if (h_errno == TRY_AGAIN)
			nflag = 1;
	}
	printf("%s (%s) at ", host, inet_ntoa(addr->sin_addr));
	if (sdl->sdl_alen) {
		if ((sdl->sdl_type == IFT_ETHER ||
		    sdl->sdl_type == IFT_L2VLAN ||
		    sdl->sdl_type == IFT_BRIDGE) &&
		    sdl->sdl_alen == ETHER_ADDR_LEN)
			printf("%s", ether_ntoa((struct ether_addr *)LLADDR(sdl)));
		else {
			int n = sdl->sdl_nlen > 0 ? sdl->sdl_nlen + 1 : 0;

			printf("%s", link_ntoa(sdl) + n);
		}
	} else
		printf("(incomplete)");
	if (if_indextoname(sdl->sdl_index, ifname) != NULL)
		printf(" on %s", ifname);
	if (rtm->rtm_rmx.rmx_expire == 0)
		printf(" permanent");
	if (addr->sin_other & SIN_PROXY)
		printf(" published (proxy only)");
	if (rtm->rtm_addrs & RTA_NETMASK) {
		addr = (struct sockaddr_inarp *)
			(SA_SIZE(sdl) + (char *)sdl);
		if (addr->sin_addr.s_addr == 0xffffffff)
			printf(" published");
		if (addr->sin_len != 8)
			printf("(weird)");
	}
        switch(sdl->sdl_type) {
	case IFT_ETHER:
                printf(" [ethernet]");
                break;
	case IFT_ISO88025:
                printf(" [token-ring]");
		trld = SDL_ISO88025(sdl);
		if (trld->trld_rcf != 0) {
			printf(" rt=%x", ntohs(trld->trld_rcf));
			for (seg = 0;
			     seg < ((TR_RCF_RIFLEN(trld->trld_rcf) - 2 ) / 2);
			     seg++) 
				printf(":%x", ntohs(*(trld->trld_route[seg])));
		}
                break;
	case IFT_FDDI:
                printf(" [fddi]");
                break;
	case IFT_ATM:
                printf(" [atm]");
                break;
	case IFT_L2VLAN:
		printf(" [vlan]");
		break;
	case IFT_IEEE1394:
                printf(" [firewire]");
                break;
	case IFT_BRIDGE:
		printf(" [bridge]");
		break;
	default:
		break;
        }
		
	printf("\n");

}

/*
 * Nuke an arp entry
 */
static void
nuke_entry(struct sockaddr_dl *sdl __unused,
	struct sockaddr_inarp *addr, struct rt_msghdr *rtm __unused)
{
	char ip[20];

	snprintf(ip, sizeof(ip), "%s", inet_ntoa(addr->sin_addr));
	(void)delete(ip, 0);
}

static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
		"usage: arp [-n] [-i interface] hostname",
		"       arp [-n] [-i interface] -a",
		"       arp -d hostname [pub]",
		"       arp -d -a",
		"       arp -s hostname ether_addr [temp] [pub]",
		"       arp -S hostname ether_addr [temp] [pub]",
		"       arp -f filename");
	exit(1);
}

static struct rt_msghdr *
rtmsg(int cmd, struct sockaddr_inarp *dst, struct sockaddr_dl *sdl)
{
	static int seq;
	int rlen;
	int l;
	struct sockaddr_in so_mask;
	static int s = -1;
	static pid_t pid;

	static struct	{
		struct	rt_msghdr m_rtm;
		char	m_space[512];
	}	m_rtmsg;

	struct rt_msghdr *rtm = &m_rtmsg.m_rtm;
	char *cp = m_rtmsg.m_space;

	if (s < 0) {	/* first time: open socket, get pid */
		s = socket(PF_ROUTE, SOCK_RAW, 0);
		if (s < 0)
			err(1, "socket");
		pid = getpid();
	}
	bzero(&so_mask, sizeof(so_mask));
	so_mask.sin_len = 8;
	so_mask.sin_addr.s_addr = 0xffffffff;

	errno = 0;
	/*
	 * XXX RTM_DELETE relies on a previous RTM_GET to fill the buffer
	 * appropriately.
	 */
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
		dst->sin_other = 0;
		if (doing_proxy) {
			if (proxy_only)
				dst->sin_other = SIN_PROXY;
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
	if ((s) != NULL && rtm->rtm_addrs & (w)) { \
		bcopy((s), cp, sizeof(*(s))); cp += SA_SIZE(s);}

	NEXTADDR(RTA_DST, dst);
	NEXTADDR(RTA_GATEWAY, sdl);
	NEXTADDR(RTA_NETMASK, &so_mask);

	rtm->rtm_msglen = cp - (char *)&m_rtmsg;
doit:
	l = rtm->rtm_msglen;
	rtm->rtm_seq = ++seq;
	rtm->rtm_type = cmd;
	if ((rlen = write(s, (char *)&m_rtmsg, l)) < 0) {
		if (errno != ESRCH || cmd != RTM_DELETE) {
			warn("writing to routing socket");
			return (NULL);
		}
	}
	do {
		l = read(s, (char *)&m_rtmsg, sizeof(m_rtmsg));
	} while (l > 0 && (rtm->rtm_seq != seq || rtm->rtm_pid != pid));
	if (l < 0)
		warn("read from routing socket");
	return (rtm);
}

/*
 * get_ether_addr - get the hardware address of an interface on the
 * the same subnet as ipaddr.
 */
#define MAX_IFS		32

static int
get_ether_addr(in_addr_t ipaddr, struct ether_addr *hwaddr)
{
	struct ifreq *ifr, *ifend, *ifp;
	in_addr_t ina, mask;
	struct sockaddr_dl *dla;
	struct ifreq ifreq;
	struct ifconf ifc;
	struct ifreq ifs[MAX_IFS];
	int sock;
	int retval = 0;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		err(1, "socket");

	ifc.ifc_len = sizeof(ifs);
	ifc.ifc_req = ifs;
	if (ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
		warnx("ioctl(SIOCGIFCONF)");
		goto done;
	}

#define NEXTIFR(i)						\
    ((struct ifreq *)((char *)&(i)->ifr_addr			\
	+ MAX((i)->ifr_addr.sa_len, sizeof((i)->ifr_addr))) )

	/*
	 * Scan through looking for an interface with an Internet
	 * address on the same subnet as `ipaddr'.
	 */
	ifend = (struct ifreq *)(ifc.ifc_buf + ifc.ifc_len);
	for (ifr = ifc.ifc_req; ifr < ifend; ifr = NEXTIFR(ifr) ) {
		if (ifr->ifr_addr.sa_family != AF_INET)
			continue;
		/* XXX can't we use *ifr instead of ifreq ? */
		strncpy(ifreq.ifr_name, ifr->ifr_name,
			sizeof(ifreq.ifr_name));
		/*
		 * Check that the interface is up,
		 * and not point-to-point or loopback.
		 */
		if (ioctl(sock, SIOCGIFFLAGS, &ifreq) < 0)
			continue;
		if ((ifreq.ifr_flags &
		     (IFF_UP|IFF_BROADCAST|IFF_POINTOPOINT|
				IFF_LOOPBACK|IFF_NOARP))
		     != (IFF_UP|IFF_BROADCAST))
			continue;
		/*
		 * Get its netmask and check that it's on 
		 * the right subnet.
		 */
		if (ioctl(sock, SIOCGIFNETMASK, &ifreq) < 0)
			continue;
		mask = ((struct sockaddr_in *)
			&ifreq.ifr_addr)->sin_addr.s_addr;
		ina = ((struct sockaddr_in *)
			&ifr->ifr_addr)->sin_addr.s_addr;
		if ((ipaddr & mask) == (ina & mask))
			break; /* ok, we got it! */
	}

	if (ifr >= ifend)
		goto done;

	/*
	 * Now scan through again looking for a link-level address
	 * for this interface.
	 */
	ifp = ifr;
	for (ifr = ifc.ifc_req; ifr < ifend; ifr = NEXTIFR(ifr))
		if (strcmp(ifp->ifr_name, ifr->ifr_name) == 0 &&
		    ifr->ifr_addr.sa_family == AF_LINK)
			break;
	if (ifr >= ifend)
		goto done;
	/*
	 * Found the link-level address - copy it out
	 */
	dla = (struct sockaddr_dl *) &ifr->ifr_addr;
	memcpy(hwaddr,  LLADDR(dla), dla->sdl_alen);
	printf("using interface %s for proxy with address ",
		ifp->ifr_name);
	printf("%s\n", ether_ntoa(hwaddr));
	retval = dla->sdl_alen;
done:
	close(sock);
	return (retval);
}
