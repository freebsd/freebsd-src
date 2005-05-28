/*
 * Copyright (c) 1983, 1993
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
static const char copyright[] =
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)ifconfig.c	8.2 (Berkeley) 2/16/94";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/module.h>
#include <sys/linker.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

/* IP */
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifdef INET6
#include <netinet6/nd6.h>	/* Define ND6_INFINITE_LIFETIME */
#endif

#ifndef NO_IPX
/* IPX */
#define	IPXIP
#define IPTUNNEL
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

/* Appletalk */
#include <netatalk/at.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>

#include "ifconfig.h"

/*
 * Since "struct ifreq" is composed of various union members, callers
 * should pay special attention to interprete the value.
 * (.e.g. little/big endian difference in the structure.)
 */
struct	ifreq		ifr, ridreq;
struct	ifaliasreq	addreq;
#ifdef INET6
struct	in6_ifreq	in6_ridreq;
struct	in6_aliasreq	in6_addreq = 
  { { 0 }, 
    { 0 }, 
    { 0 }, 
    { 0 }, 
    0, 
    { 0, 0, ND6_INFINITE_LIFETIME, ND6_INFINITE_LIFETIME } };
#endif
struct	sockaddr_in	netmask;
struct	netrange	at_nr;		/* AppleTalk net range */

char	name[IFNAMSIZ];
int	flags;
int	setaddr;
int	setipdst;
int	setmask;
int	doalias;
int	clearaddr;
int	newaddr = 1;
#ifdef INET6
static	int ip6lifetime;
#endif

struct	afswtch;

int supmedia = 0;
int listcloners = 0;
int printname = 0;		/* Print the name of the created interface. */

#ifdef INET6
char	addr_buf[MAXHOSTNAMELEN *2 + 1];	/*for getnameinfo()*/
#endif

void	Perror(const char *cmd);
void	checkatrange(struct sockaddr_at *);
int	ifconfig(int argc, char *const *argv, const struct afswtch *afp);
void	notealias(const char *, int, int, const struct afswtch *afp);
void	list_cloners(void);
void	printb(const char *s, unsigned value, const char *bits);
void	rt_xaddrs(caddr_t, caddr_t, struct rt_addrinfo *);
void	status(const struct afswtch *afp, int addrcount,
		    struct sockaddr_dl *sdl, struct if_msghdr *ifm,
		    struct ifa_msghdr *ifam);
void	tunnel_status(int s);
void	usage(void);
void	ifmaybeload(char *name);

#ifdef INET6
void	in6_fillscopeid(struct sockaddr_in6 *sin6);
int	prefix(void *, int);
static	char *sec2str(time_t);
int	explicit_prefix = 0;
#endif

typedef	void c_func(const char *cmd, int arg, int s, const struct afswtch *afp);
typedef	void c_func2(const char *arg, const char *arg2, int s, const struct afswtch *afp);
c_func	setatphase, setatrange;
c_func	setifaddr, setifbroadaddr, setifdstaddr, setifnetmask;
c_func2	settunnel;
c_func	deletetunnel;
#ifdef INET6
c_func	setifprefixlen;
c_func	setip6flags;
c_func  setip6pltime;
c_func  setip6vltime;
c_func2	setip6lifetime;
c_func	setip6eui64;
#endif
c_func	setifipdst;
c_func	setifflags, setifmetric, setifmtu, setifcap;
c_func	clone_destroy;
c_func	setifname;


void clone_create(void);


#define	NEXTARG		0xffffff
#define	NEXTARG2	0xfffffe

const
struct	cmd {
	const	char *c_name;
	int	c_parameter;		/* NEXTARG means next argv */
	void	(*c_func)(const char *, int, int, const struct afswtch *afp);
	void	(*c_func2)(const char *, const char *, int, const struct afswtch *afp);
} cmds[] = {
	{ "up",		IFF_UP,		setifflags } ,
	{ "down",	-IFF_UP,	setifflags },
	{ "arp",	-IFF_NOARP,	setifflags },
	{ "-arp",	IFF_NOARP,	setifflags },
	{ "debug",	IFF_DEBUG,	setifflags },
	{ "-debug",	-IFF_DEBUG,	setifflags },
	{ "promisc",	IFF_PPROMISC,	setifflags },
	{ "-promisc",	-IFF_PPROMISC,	setifflags },
	{ "add",	IFF_UP,		notealias },
	{ "alias",	IFF_UP,		notealias },
	{ "-alias",	-IFF_UP,	notealias },
	{ "delete",	-IFF_UP,	notealias },
	{ "remove",	-IFF_UP,	notealias },
#ifdef notdef
#define	EN_SWABIPS	0x1000
	{ "swabips",	EN_SWABIPS,	setifflags },
	{ "-swabips",	-EN_SWABIPS,	setifflags },
#endif
	{ "netmask",	NEXTARG,	setifnetmask },
#ifdef INET6
	{ "prefixlen",	NEXTARG,	setifprefixlen },
	{ "anycast",	IN6_IFF_ANYCAST, setip6flags },
	{ "tentative",	IN6_IFF_TENTATIVE, setip6flags },
	{ "-tentative",	-IN6_IFF_TENTATIVE, setip6flags },
	{ "deprecated",	IN6_IFF_DEPRECATED, setip6flags },
	{ "-deprecated", -IN6_IFF_DEPRECATED, setip6flags },
	{ "autoconf",	IN6_IFF_AUTOCONF, setip6flags },
	{ "-autoconf",	-IN6_IFF_AUTOCONF, setip6flags },
	{ "pltime",     NEXTARG,        setip6pltime },
	{ "vltime",     NEXTARG,        setip6vltime },
	{ "eui64",	0,		setip6eui64 },
#endif
	{ "range",	NEXTARG,	setatrange },
	{ "phase",	NEXTARG,	setatphase },
	{ "metric",	NEXTARG,	setifmetric },
	{ "broadcast",	NEXTARG,	setifbroadaddr },
	{ "ipdst",	NEXTARG,	setifipdst },
	{ "tunnel",	NEXTARG2,	NULL,	settunnel },
	{ "deletetunnel", 0,		deletetunnel },
	{ "link0",	IFF_LINK0,	setifflags },
	{ "-link0",	-IFF_LINK0,	setifflags },
	{ "link1",	IFF_LINK1,	setifflags },
	{ "-link1",	-IFF_LINK1,	setifflags },
	{ "link2",	IFF_LINK2,	setifflags },
	{ "-link2",	-IFF_LINK2,	setifflags },
	{ "monitor",	IFF_MONITOR,	setifflags },
	{ "-monitor",	-IFF_MONITOR,	setifflags },
	{ "staticarp",	IFF_STATICARP,	setifflags },
	{ "-staticarp",	-IFF_STATICARP,	setifflags },
#ifdef USE_IF_MEDIA
	{ "media",	NEXTARG,	setmedia },
	{ "mode",	NEXTARG,	setmediamode },
	{ "mediaopt",	NEXTARG,	setmediaopt },
	{ "-mediaopt",	NEXTARG,	unsetmediaopt },
#endif
#ifdef USE_PFSYNC
	{ "syncif",	NEXTARG,	setpfsync_syncif },
	{ "maxupd",	NEXTARG,	setpfsync_maxupd },
	{ "-syncif",	1,		unsetpfsync_syncif },
#endif
#ifdef USE_VLANS
	{ "vlan",	NEXTARG,	setvlantag },
	{ "vlandev",	NEXTARG,	setvlandev },
	{ "-vlandev",	NEXTARG,	unsetvlandev },
#endif
#if 0
	/* XXX `create' special-cased below */
	{"create",	0,		clone_create },
	{"plumb",	0,		clone_create },
#endif
	{"destroy",	0,		clone_destroy },
	{"unplumb",	0,		clone_destroy },
#ifdef USE_IEEE80211
	{ "ssid",	NEXTARG,	set80211ssid },
	{ "nwid",	NEXTARG,	set80211ssid },
	{ "stationname", NEXTARG,	set80211stationname },
	{ "station",	NEXTARG,	set80211stationname },	/* BSD/OS */
	{ "channel",	NEXTARG,	set80211channel },
	{ "authmode",	NEXTARG,	set80211authmode },
	{ "powersavemode", NEXTARG,	set80211powersavemode },
	{ "powersave",	1,		set80211powersave },
	{ "-powersave",	0,		set80211powersave },
	{ "powersavesleep", NEXTARG,	set80211powersavesleep },
	{ "wepmode",	NEXTARG,	set80211wepmode },
	{ "wep",	1,		set80211wep },
	{ "-wep",	0,		set80211wep },
	{ "weptxkey",	NEXTARG,	set80211weptxkey },
	{ "wepkey",	NEXTARG,	set80211wepkey },
	{ "nwkey",	NEXTARG,	set80211nwkey },	/* NetBSD */
	{ "-nwkey",	0,		set80211wep },		/* NetBSD */
	{ "rtsthreshold",NEXTARG,	set80211rtsthreshold },
	{ "protmode",	NEXTARG,	set80211protmode },
	{ "txpower",	NEXTARG,	set80211txpower },
#endif
#ifdef USE_MAC
	{ "maclabel",	NEXTARG,	setifmaclabel },
#endif
#ifdef USE_CARP
	{ "advbase",	NEXTARG,	setcarp_advbase },
	{ "advskew",	NEXTARG,	setcarp_advskew },
	{ "pass",	NEXTARG,	setcarp_passwd },
	{ "vhid",	NEXTARG,	setcarp_vhid },
#endif
	{ "rxcsum",	IFCAP_RXCSUM,	setifcap },
	{ "-rxcsum",	-IFCAP_RXCSUM,	setifcap },
	{ "txcsum",	IFCAP_TXCSUM,	setifcap },
	{ "-txcsum",	-IFCAP_TXCSUM,	setifcap },
	{ "netcons",	IFCAP_NETCONS,	setifcap },
	{ "-netcons",	-IFCAP_NETCONS,	setifcap },
	{ "polling",	IFCAP_POLLING,	setifcap },
	{ "-polling",	-IFCAP_POLLING,	setifcap },
	{ "vlanmtu",	IFCAP_VLAN_MTU,		setifcap },
	{ "-vlanmtu",	-IFCAP_VLAN_MTU,	setifcap },
	{ "vlanhwtag",	IFCAP_VLAN_HWTAGGING,	setifcap },
	{ "-vlanhwtag",	-IFCAP_VLAN_HWTAGGING,	setifcap },
	{ "normal",	-IFF_LINK0,	setifflags },
	{ "compress",	IFF_LINK0,	setifflags },
	{ "noicmp",	IFF_LINK1,	setifflags },
	{ "mtu",	NEXTARG,	setifmtu },
	{ "name",	NEXTARG,	setifname },
	{ 0,		0,		setifaddr },
	{ 0,		0,		setifdstaddr },
};

/*
 * XNS support liberally adapted from code written at the University of
 * Maryland principally by James O'Toole and Chris Torek.
 */
typedef	void af_status(int, struct rt_addrinfo *);
typedef	void af_getaddr(const char *, int);
typedef void af_getprefix(const char *, int);

af_status	in_status, at_status, link_status;
af_getaddr	in_getaddr, at_getaddr, link_getaddr;

#ifndef NO_IPX
af_status	ipx_status;
af_getaddr	ipx_getaddr;
#endif

#ifdef INET6
af_status	in6_status;
af_getaddr	in6_getaddr;
af_getprefix	in6_getprefix;
#endif /*INET6*/

/* Known address families */
const
struct	afswtch {
	const char *af_name;
	short af_af;
	af_status *af_status;
	af_getaddr *af_getaddr;
	af_getprefix *af_getprefix;
	u_long af_difaddr;
	u_long af_aifaddr;
	caddr_t af_ridreq;
	caddr_t af_addreq;
} afs[] = {
#define C(x) ((caddr_t) &x)
	{ "inet", AF_INET, in_status, in_getaddr, NULL,
	     SIOCDIFADDR, SIOCAIFADDR, C(ridreq), C(addreq) },
#ifdef INET6
	{ "inet6", AF_INET6, in6_status, in6_getaddr, in6_getprefix,
	     SIOCDIFADDR_IN6, SIOCAIFADDR_IN6,
	     C(in6_ridreq), C(in6_addreq) },
#endif /*INET6*/
#ifndef NO_IPX
	{ "ipx", AF_IPX, ipx_status, ipx_getaddr, NULL,
	     SIOCDIFADDR, SIOCAIFADDR, C(ridreq), C(addreq) },
#endif
	{ "atalk", AF_APPLETALK, at_status, at_getaddr, NULL,
	     SIOCDIFADDR, SIOCAIFADDR, C(addreq), C(addreq) },
	{ "link", AF_LINK, link_status, link_getaddr, NULL,
	     0, SIOCSIFLLADDR, NULL, C(ridreq) },
	{ "ether", AF_LINK, link_status, link_getaddr, NULL,
	     0, SIOCSIFLLADDR, NULL, C(ridreq) },
	{ "lladdr", AF_LINK, link_status, link_getaddr, NULL,
	     0, SIOCSIFLLADDR, NULL, C(ridreq) },
#if 0	/* XXX conflicts with the media command */
#ifdef USE_IF_MEDIA
	{ "media", AF_UNSPEC, media_status, NULL, NULL, }, /* XXX not real!! */
#endif
#ifdef USE_VLANS
	{ "vlan", AF_UNSPEC, vlan_status, NULL, NULL, },  /* XXX not real!! */
#endif
#ifdef USE_IEEE80211
	{ "ieee80211", AF_UNSPEC, ieee80211_status, NULL, NULL, },  /* XXX not real!! */
#endif
#ifdef USE_CARP
	{ "carp", AF_UNSPEC, carp_status, NULL, NULL, },  /* XXX not real!! */
#endif
#ifdef USE_MAC
	{ "maclabel", AF_UNSPEC, maclabel_status, NULL, NULL, },
#endif
#endif
	{ 0,	0,	    0,		0 }
};

/*
 * Expand the compacted form of addresses as returned via the
 * configuration read via sysctl().
 */

void
rt_xaddrs(caddr_t cp, caddr_t cplim, struct rt_addrinfo *rtinfo)
{
	struct sockaddr *sa;
	int i;

	memset(rtinfo->rti_info, 0, sizeof(rtinfo->rti_info));
	for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
		if ((rtinfo->rti_addrs & (1 << i)) == 0)
			continue;
		rtinfo->rti_info[i] = sa = (struct sockaddr *)cp;
		cp += SA_SIZE(sa);
	}
}


void
usage(void)
{
#ifndef INET6
	fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
	"usage: ifconfig interface address_family [address [dest_address]]",
	"                [parameters]",
	"       ifconfig -C",
	"       ifconfig interface create",
	"       ifconfig -a [-d] [-m] [-u] [address_family]",
	"       ifconfig -l [-d] [-u] [address_family]",
	"       ifconfig [-d] [-m] [-u]");
#else
	fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
	"usage: ifconfig [-L] interface address_family [address [dest_address]]",
	"                [parameters]",
	"       ifconfig -C",
	"       ifconfig interface create",
	"       ifconfig -a [-L] [-d] [-m] [-u] [address_family]",
	"       ifconfig -l [-d] [-u] [address_family]",
	"       ifconfig [-L] [-d] [-m] [-u]");
#endif
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c;
	int all, namesonly, downonly, uponly;
	int need_nl = 0, count = 0;
	const struct afswtch *afp = 0;
	int addrcount, ifindex;
	struct	if_msghdr *ifm, *nextifm;
	struct	ifa_msghdr *ifam;
	struct	sockaddr_dl *sdl;
	char	*buf, *lim, *next;
	size_t needed;
	int mib[6];

	/* Parse leading line options */
	all = downonly = uponly = namesonly = 0;
	while ((c = getopt(argc, argv, "adlmuC"
#ifdef INET6
					"L"
#endif
			)) != -1) {
		switch (c) {
		case 'a':	/* scan all interfaces */
			all++;
			break;
		case 'd':	/* restrict scan to "down" interfaces */
			downonly++;
			break;
		case 'l':	/* scan interface names only */
			namesonly++;
			break;
		case 'm':	/* show media choices in status */
			supmedia = 1;
			break;
		case 'u':	/* restrict scan to "up" interfaces */
			uponly++;
			break;
		case 'C':
			listcloners = 1;
			break;
#ifdef INET6
		case 'L':
			ip6lifetime++;	/* print IPv6 address lifetime */
			break;
#endif
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (listcloners) {
		/* -C must be solitary */
		if (all || supmedia || uponly || downonly || namesonly ||
		    argc > 0)
			usage();
		
		list_cloners();
		exit(0);
	}

	/* -l cannot be used with -a or -m */
	if (namesonly && (all || supmedia))
		usage();

	/* nonsense.. */
	if (uponly && downonly)
		usage();

	/* no arguments is equivalent to '-a' */
	if (!namesonly && argc < 1)
		all = 1;

	/* -a and -l allow an address family arg to limit the output */
	if (all || namesonly) {
		if (argc > 1)
			usage();

		ifindex = 0;
		if (argc == 1) {
			for (afp = afs; afp->af_name; afp++)
				if (strcmp(afp->af_name, *argv) == 0) {
					argc--, argv++;
					break;
				}
			if (afp->af_name == NULL)
				usage();
			/* leave with afp non-zero */
		}
	} else {
		/* not listing, need an argument */
		if (argc < 1)
			usage();

		strncpy(name, *argv, sizeof(name));
		argc--, argv++;

		/* check and maybe load support for this interface */
		ifmaybeload(name);

		/*
		 * NOTE:  We must special-case the `create' command right
		 * here as we would otherwise fail when trying to find
		 * the interface.
		 */
		if (argc > 0 && (strcmp(argv[0], "create") == 0 ||
		    strcmp(argv[0], "plumb") == 0)) {
			clone_create();
			argc--, argv++;
			if (argc == 0)
				goto end;
		}
		ifindex = if_nametoindex(name);
		if (ifindex == 0)
			errx(1, "interface %s does not exist", name);
	}

	/* Check for address family */
	if (argc > 0) {
		for (afp = afs; afp->af_name; afp++)
			if (strcmp(afp->af_name, *argv) == 0) {
				argc--, argv++;
				break;
			}
		if (afp->af_name == NULL)
			afp = NULL;	/* not a family, NULL */
	}

retry:
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;			/* address family */
	mib[4] = NET_RT_IFLIST;
	mib[5] = ifindex;		/* interface index */

	/* if particular family specified, only ask about it */
	if (afp)
		mib[3] = afp->af_af;

	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		errx(1, "iflist-sysctl-estimate");
	if ((buf = malloc(needed)) == NULL)
		errx(1, "malloc");
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
		if (errno == ENOMEM && count++ < 10) {
			warnx("Routing table grew, retrying");
			free(buf);
			sleep(1);
			goto retry;
		}
		errx(1, "actual retrieval of interface table");
	}
	lim = buf + needed;

	next = buf;
	while (next < lim) {

		ifm = (struct if_msghdr *)next;
		
		if (ifm->ifm_type == RTM_IFINFO) {
			if (ifm->ifm_data.ifi_datalen == 0)
				ifm->ifm_data.ifi_datalen = sizeof(struct if_data);
			sdl = (struct sockaddr_dl *)((char *)ifm + sizeof(struct if_msghdr) -
			     sizeof(struct if_data) + ifm->ifm_data.ifi_datalen);
			flags = ifm->ifm_flags;
		} else {
			fprintf(stderr, "out of sync parsing NET_RT_IFLIST\n");
			fprintf(stderr, "expected %d, got %d\n", RTM_IFINFO,
				ifm->ifm_type);
			fprintf(stderr, "msglen = %d\n", ifm->ifm_msglen);
			fprintf(stderr, "buf:%p, next:%p, lim:%p\n", buf, next,
				lim);
			exit (1);
		}

		next += ifm->ifm_msglen;
		ifam = NULL;
		addrcount = 0;
		while (next < lim) {

			nextifm = (struct if_msghdr *)next;

			if (nextifm->ifm_type != RTM_NEWADDR)
				break;

			if (ifam == NULL)
				ifam = (struct ifa_msghdr *)nextifm;

			addrcount++;
			next += nextifm->ifm_msglen;
		}
		memcpy(name, sdl->sdl_data,
		    sizeof(name) < sdl->sdl_nlen ?
		    sizeof(name)-1 : sdl->sdl_nlen);
		name[sizeof(name) < sdl->sdl_nlen ?
		    sizeof(name)-1 : sdl->sdl_nlen] = '\0';

		if (all || namesonly) {
			if (uponly)
				if ((flags & IFF_UP) == 0)
					continue; /* not up */
			if (downonly)
				if (flags & IFF_UP)
					continue; /* not down */
			if (namesonly) {
				if (afp == NULL ||
					afp->af_status != link_status ||
					sdl->sdl_type == IFT_ETHER) {
					if (need_nl)
						putchar(' ');
					fputs(name, stdout);
					need_nl++;
				}
				continue;
			}
		}

		if (argc > 0)
			ifconfig(argc, argv, afp);
		else
			status(afp, addrcount, sdl, ifm, ifam);
	}
	free(buf);

	if (namesonly && need_nl > 0)
		putchar('\n');
end:
	if (printname)
		printf("%s\n", name);

	exit (0);
}

int
ifconfig(int argc, char *const *argv, const struct afswtch *afp)
{
	int af, s;

	if (afp == NULL)
		afp = &afs[0];
	af = afp->af_af == AF_LINK ? AF_INET : afp->af_af;
	ifr.ifr_addr.sa_family = af;
	strncpy(ifr.ifr_name, name, sizeof ifr.ifr_name);

	if ((s = socket(af, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");

	while (argc > 0) {
		const struct cmd *p;

		for (p = cmds; p->c_name; p++)
			if (strcmp(*argv, p->c_name) == 0)
				break;
		if (p->c_name == 0 && setaddr)
			p++;	/* got src, do dst */
		if (p->c_func || p->c_func2) {
			if (p->c_parameter == NEXTARG) {
				if (argv[1] == NULL)
					errx(1, "'%s' requires argument",
					    p->c_name);
				(*p->c_func)(argv[1], 0, s, afp);
				argc--, argv++;
			} else if (p->c_parameter == NEXTARG2) {
				if (argc < 3)
					errx(1, "'%s' requires 2 arguments",
					    p->c_name);
				(*p->c_func2)(argv[1], argv[2], s, afp);
				argc -= 2, argv += 2;
			} else
				(*p->c_func)(*argv, p->c_parameter, s, afp);
		}
		argc--, argv++;
	}
#ifdef INET6
	if (af == AF_INET6 && explicit_prefix == 0) {
		/* Aggregatable address architecture defines all prefixes
		   are 64. So, it is convenient to set prefixlen to 64 if
		   it is not specified. */
		setifprefixlen("64", 0, s, afp);
		/* in6_getprefix("64", MASK) if MASK is available here... */
	}
#endif
#ifndef NO_IPX
	if (setipdst && af == AF_IPX) {
		struct ipxip_req rq;
		int size = sizeof(rq);

		rq.rq_ipx = addreq.ifra_addr;
		rq.rq_ip = addreq.ifra_dstaddr;

		if (setsockopt(s, 0, SO_IPXIP_ROUTE, &rq, size) < 0)
			Perror("Encapsulation Routing");
	}
#endif
	if (af == AF_APPLETALK)
		checkatrange((struct sockaddr_at *) &addreq.ifra_addr);
	if (clearaddr) {
		if (afp->af_ridreq == NULL || afp->af_difaddr == 0) {
			warnx("interface %s cannot change %s addresses!",
			      name, afp->af_name);
			clearaddr = 0;
		}
	}
	if (clearaddr) {
		int ret;
		strncpy(afp->af_ridreq, name, sizeof ifr.ifr_name);
		if ((ret = ioctl(s, afp->af_difaddr, afp->af_ridreq)) < 0) {
			if (errno == EADDRNOTAVAIL && (doalias >= 0)) {
				/* means no previous address for interface */
			} else
				Perror("ioctl (SIOCDIFADDR)");
		}
	}
	if (newaddr) {
		if (afp->af_addreq == NULL || afp->af_aifaddr == 0) {
			warnx("interface %s cannot change %s addresses!",
			      name, afp->af_name);
			newaddr = 0;
		}
	}
	if (newaddr && (setaddr || setmask)) {
		strncpy(afp->af_addreq, name, sizeof ifr.ifr_name);
		if (ioctl(s, afp->af_aifaddr, afp->af_addreq) < 0)
			Perror("ioctl (SIOCAIFADDR)");
	}
	close(s);
	return(0);
}
#define RIDADDR 0
#define ADDR	1
#define MASK	2
#define DSTADDR	3

/*ARGSUSED*/
void
setifaddr(const char *addr, int param, int s, const struct afswtch *afp)
{
	if (*afp->af_getaddr == NULL)
		return;
	/*
	 * Delay the ioctl to set the interface addr until flags are all set.
	 * The address interpretation may depend on the flags,
	 * and the flags may change when the address is set.
	 */
	setaddr++;
	if (doalias == 0 && afp->af_af != AF_LINK)
		clearaddr = 1;
	(*afp->af_getaddr)(addr, (doalias >= 0 ? ADDR : RIDADDR));
}

void
settunnel(const char *src, const char *dst, int s, const struct afswtch *afp)
{
	struct addrinfo hints, *srcres, *dstres;
	struct ifaliasreq addreq;
	int ecode;
#ifdef INET6
	struct in6_aliasreq in6_addreq; 
#endif

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = afp->af_af;

	if ((ecode = getaddrinfo(src, NULL, NULL, &srcres)) != 0)
		errx(1, "error in parsing address string: %s",
		    gai_strerror(ecode));

	if ((ecode = getaddrinfo(dst, NULL, NULL, &dstres)) != 0)  
		errx(1, "error in parsing address string: %s",
		    gai_strerror(ecode));

	if (srcres->ai_addr->sa_family != dstres->ai_addr->sa_family)
		errx(1,
		    "source and destination address families do not match");

	switch (srcres->ai_addr->sa_family) {
	case AF_INET:
		memset(&addreq, 0, sizeof(addreq));
		strncpy(addreq.ifra_name, name, IFNAMSIZ);
		memcpy(&addreq.ifra_addr, srcres->ai_addr,
		    srcres->ai_addr->sa_len);
		memcpy(&addreq.ifra_dstaddr, dstres->ai_addr,
		    dstres->ai_addr->sa_len);

		if (ioctl(s, SIOCSIFPHYADDR, &addreq) < 0)
			warn("SIOCSIFPHYADDR");
		break;

#ifdef INET6
	case AF_INET6:
		memset(&in6_addreq, 0, sizeof(in6_addreq));
		strncpy(in6_addreq.ifra_name, name, IFNAMSIZ);
		memcpy(&in6_addreq.ifra_addr, srcres->ai_addr,
		    srcres->ai_addr->sa_len);
		memcpy(&in6_addreq.ifra_dstaddr, dstres->ai_addr,
		    dstres->ai_addr->sa_len);

		if (ioctl(s, SIOCSIFPHYADDR_IN6, &in6_addreq) < 0)
			warn("SIOCSIFPHYADDR_IN6");
		break;
#endif /* INET6 */

	default:
		warn("address family not supported");
	}

	freeaddrinfo(srcres);
	freeaddrinfo(dstres);
}

/* ARGSUSED */
void
deletetunnel(const char *vname, int param, int s, const struct afswtch *afp)
{

	if (ioctl(s, SIOCDIFPHYADDR, &ifr) < 0)
		err(1, "SIOCDIFPHYADDR");
}

void
setifnetmask(const char *addr, int dummy __unused, int s,
    const struct afswtch *afp)
{
	if (*afp->af_getaddr == NULL)
		return;
	setmask++;
	(*afp->af_getaddr)(addr, MASK);
}

#ifdef INET6
void
setifprefixlen(const char *addr, int dummy __unused, int s,
    const struct afswtch *afp)
{
        if (*afp->af_getprefix)
                (*afp->af_getprefix)(addr, MASK);
	explicit_prefix = 1;
}

void
setip6flags(const char *dummyaddr __unused, int flag, int dummysoc __unused,
    const struct afswtch *afp)
{
	if (afp->af_af != AF_INET6)
		err(1, "address flags can be set only for inet6 addresses");

	if (flag < 0)
		in6_addreq.ifra_flags &= ~(-flag);
	else
		in6_addreq.ifra_flags |= flag;
}

void
setip6pltime(const char *seconds, int dummy __unused, int s, 
    const struct afswtch *afp)
{
	setip6lifetime("pltime", seconds, s, afp);
}

void
setip6vltime(const char *seconds, int dummy __unused, int s, 
    const struct afswtch *afp)
{
	setip6lifetime("vltime", seconds, s, afp);
}

void
setip6lifetime(const char *cmd, const char *val, int s, 
    const struct afswtch *afp)
{
	time_t newval, t;
	char *ep;

	t = time(NULL);
	newval = (time_t)strtoul(val, &ep, 0);
	if (val == ep)
		errx(1, "invalid %s", cmd);
	if (afp->af_af != AF_INET6)
		errx(1, "%s not allowed for the AF", cmd);
	if (strcmp(cmd, "vltime") == 0) {
		in6_addreq.ifra_lifetime.ia6t_expire = t + newval;
		in6_addreq.ifra_lifetime.ia6t_vltime = newval;
	} else if (strcmp(cmd, "pltime") == 0) {
		in6_addreq.ifra_lifetime.ia6t_preferred = t + newval;
		in6_addreq.ifra_lifetime.ia6t_pltime = newval;
	}
}

void
setip6eui64(const char *cmd, int dummy __unused, int s,
    const struct afswtch *afp)
{
	struct ifaddrs *ifap, *ifa;
	const struct sockaddr_in6 *sin6 = NULL;
	const struct in6_addr *lladdr = NULL;
	struct in6_addr *in6;

	if (afp->af_af != AF_INET6)
		errx(EXIT_FAILURE, "%s not allowed for the AF", cmd);
 	in6 = (struct in6_addr *)&in6_addreq.ifra_addr.sin6_addr;
	if (memcmp(&in6addr_any.s6_addr[8], &in6->s6_addr[8], 8) != 0)
		errx(EXIT_FAILURE, "interface index is already filled");
	if (getifaddrs(&ifap) != 0)
		err(EXIT_FAILURE, "getifaddrs");
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family == AF_INET6 &&
		    strcmp(ifa->ifa_name, name) == 0) {
			sin6 = (const struct sockaddr_in6 *)ifa->ifa_addr;
			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
				lladdr = &sin6->sin6_addr;
				break;
			}
		}
	}
	if (!lladdr)
		errx(EXIT_FAILURE, "could not determine link local address"); 

 	memcpy(&in6->s6_addr[8], &lladdr->s6_addr[8], 8);

	freeifaddrs(ifap);
}
#endif

void
setifbroadaddr(const char *addr, int dummy __unused, int s,
    const struct afswtch *afp)
{
	if (*afp->af_getaddr == NULL)
		return;
	(*afp->af_getaddr)(addr, DSTADDR);
}

void
setifipdst(const char *addr, int dummy __unused, int s,
    const struct afswtch *afp)
{
	in_getaddr(addr, DSTADDR);
	setipdst++;
	clearaddr = 0;
	newaddr = 0;
}
#define rqtosa(x) (&(((struct ifreq *)(afp->x))->ifr_addr))

void
notealias(const char *addr, int param, int s, const struct afswtch *afp)
{
	if (setaddr && doalias == 0 && param < 0)
		if (afp->af_addreq != NULL && afp->af_ridreq != NULL)
			bcopy((caddr_t)rqtosa(af_addreq),
			      (caddr_t)rqtosa(af_ridreq),
			      rqtosa(af_addreq)->sa_len);
	doalias = param;
	if (param < 0) {
		clearaddr = 1;
		newaddr = 0;
	} else
		clearaddr = 0;
}

/*ARGSUSED*/
void
setifdstaddr(const char *addr, int param __unused, int s, 
    const struct afswtch *afp)
{
	if (*afp->af_getaddr == NULL)
		return;
	(*afp->af_getaddr)(addr, DSTADDR);
}

/*
 * Note: doing an SIOCIGIFFLAGS scribbles on the union portion
 * of the ifreq structure, which may confuse other parts of ifconfig.
 * Make a private copy so we can avoid that.
 */
void
setifflags(const char *vname, int value, int s, const struct afswtch *afp)
{
	struct ifreq		my_ifr;

	bcopy((char *)&ifr, (char *)&my_ifr, sizeof(struct ifreq));

 	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&my_ifr) < 0) {
 		Perror("ioctl (SIOCGIFFLAGS)");
 		exit(1);
 	}
	strncpy(my_ifr.ifr_name, name, sizeof (my_ifr.ifr_name));
	flags = (my_ifr.ifr_flags & 0xffff) | (my_ifr.ifr_flagshigh << 16);

	if (value < 0) {
		value = -value;
		flags &= ~value;
	} else
		flags |= value;
	my_ifr.ifr_flags = flags & 0xffff;
	my_ifr.ifr_flagshigh = flags >> 16;
	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&my_ifr) < 0)
		Perror(vname);
}

void
setifcap(const char *vname, int value, int s, const struct afswtch *afp)
{

 	if (ioctl(s, SIOCGIFCAP, (caddr_t)&ifr) < 0) {
 		Perror("ioctl (SIOCGIFCAP)");
 		exit(1);
 	}
	flags = ifr.ifr_curcap;
	if (value < 0) {
		value = -value;
		flags &= ~value;
	} else
		flags |= value;
	ifr.ifr_reqcap = flags;
	if (ioctl(s, SIOCSIFCAP, (caddr_t)&ifr) < 0)
		Perror(vname);
}

void
setifmetric(const char *val, int dummy __unused, int s, 
    const struct afswtch *afp)
{
	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	ifr.ifr_metric = atoi(val);
	if (ioctl(s, SIOCSIFMETRIC, (caddr_t)&ifr) < 0)
		warn("ioctl (set metric)");
}

void
setifmtu(const char *val, int dummy __unused, int s, 
    const struct afswtch *afp)
{
	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	ifr.ifr_mtu = atoi(val);
	if (ioctl(s, SIOCSIFMTU, (caddr_t)&ifr) < 0)
		warn("ioctl (set mtu)");
}

void
setifname(const char *val, int dummy __unused, int s, 
    const struct afswtch *afp)
{
	char	*newname;

	newname = strdup(val);

	ifr.ifr_data = newname;
	if (ioctl(s, SIOCSIFNAME, (caddr_t)&ifr) < 0) {
		warn("ioctl (set name)");
		free(newname);
		return;
	}
	strlcpy(name, newname, sizeof(name));
	free(newname);

	/*
	 * Even if we just created the interface, we don't need to print
	 * its name because we just nailed it down separately.
	 */
	printname = 0;
}

#define	IFFBITS \
"\020\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5POINTOPOINT\6SMART\7RUNNING" \
"\10NOARP\11PROMISC\12ALLMULTI\13OACTIVE\14SIMPLEX\15LINK0\16LINK1\17LINK2" \
"\20MULTICAST\21POLLING\23MONITOR\24STATICARP"

#define	IFCAPBITS \
"\020\1RXCSUM\2TXCSUM\3NETCONS\4VLAN_MTU\5VLAN_HWTAGGING\6JUMBO_MTU\7POLLING"

/*
 * Print the status of the interface.  If an address family was
 * specified, show it and it only; otherwise, show them all.
 */
void
status(const struct afswtch *afp, int addrcount, struct	sockaddr_dl *sdl,
    struct if_msghdr *ifm, struct ifa_msghdr *ifam)
{
	const struct afswtch *p = NULL;
	struct	rt_addrinfo info;
	int allfamilies, s;
	struct ifstat ifs;

	if (afp == NULL) {
		allfamilies = 1;
		afp = &afs[0];
	} else
		allfamilies = 0;

	ifr.ifr_addr.sa_family = afp->af_af == AF_LINK ? AF_INET : afp->af_af;
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

	if ((s = socket(ifr.ifr_addr.sa_family, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");

	printf("%s: ", name);
	printb("flags", flags, IFFBITS);
	if (ifm->ifm_data.ifi_metric)
		printf(" metric %ld", ifm->ifm_data.ifi_metric);
	if (ifm->ifm_data.ifi_mtu)
		printf(" mtu %ld", ifm->ifm_data.ifi_mtu);
	putchar('\n');

	if (ioctl(s, SIOCGIFCAP, (caddr_t)&ifr) == 0) {
		if (ifr.ifr_curcap != 0) {
			printb("\toptions", ifr.ifr_curcap, IFCAPBITS);
			putchar('\n');
		}
		if (supmedia && ifr.ifr_reqcap != 0) {
			printf("\tcapability list:\n");
			printb("\t\t", ifr.ifr_reqcap, IFCAPBITS);
			putchar('\n');
		}
	}

	tunnel_status(s);

	while (addrcount > 0) {
		
		info.rti_addrs = ifam->ifam_addrs;

		/* Expand the compacted addresses */
		rt_xaddrs((char *)(ifam + 1), ifam->ifam_msglen + (char *)ifam,
			  &info);

		if (!allfamilies) {
			if (afp->af_af == info.rti_info[RTAX_IFA]->sa_family) {
				p = afp;
				(*p->af_status)(s, &info);
			}
		} else for (p = afs; p->af_name; p++) {
			if (p->af_af == info.rti_info[RTAX_IFA]->sa_family)
				(*p->af_status)(s, &info);
		}
		addrcount--;
		ifam = (struct ifa_msghdr *)((char *)ifam + ifam->ifam_msglen);
	}
	if (allfamilies || afp->af_status == link_status)
		link_status(s, (struct rt_addrinfo *)sdl);
#ifdef USE_IF_MEDIA
	if (allfamilies || afp->af_status == media_status)
		media_status(s, NULL);
#endif
#ifdef USE_PFSYNC
	if (allfamilies || afp->af_status == pfsync_status)
		pfsync_status(s, NULL);
#endif
#ifdef USE_VLANS
	if (allfamilies || afp->af_status == vlan_status)
		vlan_status(s, NULL);
#endif
#ifdef USE_IEEE80211
	if (allfamilies || afp->af_status == ieee80211_status)
		ieee80211_status(s, NULL);
#endif
#ifdef USE_CARP
	if (allfamilies || afp->af_status == carp_status)
		carp_status(s, NULL);
#endif
#ifdef USE_MAC
	if (allfamilies || afp->af_status == maclabel_status)
		maclabel_status(s, NULL);
#endif
	strncpy(ifs.ifs_name, name, sizeof ifs.ifs_name);
	if (ioctl(s, SIOCGIFSTATUS, &ifs) == 0) 
		printf("%s", ifs.ascii);

	if (!allfamilies && !p &&
#ifdef USE_IF_MEDIA
	    afp->af_status != media_status &&
#endif
	    afp->af_status != link_status
#ifdef USE_VLANS
	    && afp->af_status != vlan_status
#endif
#ifdef USE_CARP
	    && afp->af_status != carp_status
#endif
		)
		warnx("%s has no %s interface address!", name, afp->af_name);

	close(s);
	return;
}

void
tunnel_status(int s)
{
	char psrcaddr[NI_MAXHOST];
	char pdstaddr[NI_MAXHOST];
	u_long srccmd, dstcmd;
	struct ifreq *ifrp;
	const char *ver = "";
#ifdef INET6
	struct in6_ifreq in6_ifr;
	int s6;
#endif /* INET6 */

	psrcaddr[0] = pdstaddr[0] = '\0';

#ifdef INET6
	memset(&in6_ifr, 0, sizeof(in6_ifr));
	strncpy(in6_ifr.ifr_name, name, IFNAMSIZ);
	s6 = socket(AF_INET6, SOCK_DGRAM, 0);
	if (s6 < 0) {
		srccmd = SIOCGIFPSRCADDR;
		dstcmd = SIOCGIFPDSTADDR;
		ifrp = &ifr;
	} else {
		close(s6);
		srccmd = SIOCGIFPSRCADDR_IN6;
		dstcmd = SIOCGIFPDSTADDR_IN6;
		ifrp = (struct ifreq *)&in6_ifr;
	}
#else /* INET6 */
	srccmd = SIOCGIFPSRCADDR;
	dstcmd = SIOCGIFPDSTADDR;
	ifrp = &ifr;
#endif /* INET6 */

	if (ioctl(s, srccmd, (caddr_t)ifrp) < 0)
		return;
#ifdef INET6
	if (ifrp->ifr_addr.sa_family == AF_INET6)
		in6_fillscopeid((struct sockaddr_in6 *)&ifrp->ifr_addr);
#endif
	getnameinfo(&ifrp->ifr_addr, ifrp->ifr_addr.sa_len,
	    psrcaddr, sizeof(psrcaddr), 0, 0, NI_NUMERICHOST);
#ifdef INET6
	if (ifrp->ifr_addr.sa_family == AF_INET6)
		ver = "6";
#endif

	if (ioctl(s, dstcmd, (caddr_t)ifrp) < 0)
		return;
#ifdef INET6
	if (ifrp->ifr_addr.sa_family == AF_INET6)
		in6_fillscopeid((struct sockaddr_in6 *)&ifrp->ifr_addr);
#endif
	getnameinfo(&ifrp->ifr_addr, ifrp->ifr_addr.sa_len,
	    pdstaddr, sizeof(pdstaddr), 0, 0, NI_NUMERICHOST);

	printf("\ttunnel inet%s %s --> %s\n", ver,
	    psrcaddr, pdstaddr);
}

void
in_status(int s __unused, struct rt_addrinfo * info)
{
	struct sockaddr_in *sin, null_sin;
	
	memset(&null_sin, 0, sizeof(null_sin));

	sin = (struct sockaddr_in *)info->rti_info[RTAX_IFA];
	printf("\tinet %s ", inet_ntoa(sin->sin_addr));

	if (flags & IFF_POINTOPOINT) {
		/* note RTAX_BRD overlap with IFF_BROADCAST */
		sin = (struct sockaddr_in *)info->rti_info[RTAX_BRD];
		if (!sin)
			sin = &null_sin;
		printf("--> %s ", inet_ntoa(sin->sin_addr));
	}

	sin = (struct sockaddr_in *)info->rti_info[RTAX_NETMASK];
	if (!sin)
		sin = &null_sin;
	printf("netmask 0x%lx ", (unsigned long)ntohl(sin->sin_addr.s_addr));

	if (flags & IFF_BROADCAST) {
		/* note RTAX_BRD overlap with IFF_POINTOPOINT */
		sin = (struct sockaddr_in *)info->rti_info[RTAX_BRD];
		if (sin && sin->sin_addr.s_addr != 0)
			printf("broadcast %s", inet_ntoa(sin->sin_addr));
	}
	putchar('\n');
}

#ifdef INET6
void
in6_fillscopeid(struct sockaddr_in6 *sin6)
{
#if defined(__KAME__) && defined(KAME_SCOPEID)
	if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
		sin6->sin6_scope_id =
			ntohs(*(u_int16_t *)&sin6->sin6_addr.s6_addr[2]);
		sin6->sin6_addr.s6_addr[2] = sin6->sin6_addr.s6_addr[3] = 0;
	}
#endif
}

void
in6_status(int s __unused, struct rt_addrinfo * info)
{
	struct sockaddr_in6 *sin, null_sin;
	struct in6_ifreq ifr6;
	int s6;
	u_int32_t flags6;
	struct in6_addrlifetime lifetime;
	time_t t = time(NULL);
	int error;
	u_int32_t scopeid;

	memset(&null_sin, 0, sizeof(null_sin));

	sin = (struct sockaddr_in6 *)info->rti_info[RTAX_IFA];
	strncpy(ifr6.ifr_name, ifr.ifr_name, sizeof(ifr.ifr_name));
	if ((s6 = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		perror("ifconfig: socket");
		return;
	}
	ifr6.ifr_addr = *sin;
	if (ioctl(s6, SIOCGIFAFLAG_IN6, &ifr6) < 0) {
		perror("ifconfig: ioctl(SIOCGIFAFLAG_IN6)");
		close(s6);
		return;
	}
	flags6 = ifr6.ifr_ifru.ifru_flags6;
	memset(&lifetime, 0, sizeof(lifetime));
	ifr6.ifr_addr = *sin;
	if (ioctl(s6, SIOCGIFALIFETIME_IN6, &ifr6) < 0) {
		perror("ifconfig: ioctl(SIOCGIFALIFETIME_IN6)");
		close(s6);
		return;
	}
	lifetime = ifr6.ifr_ifru.ifru_lifetime;
	close(s6);

	/* XXX: embedded link local addr check */
	if (IN6_IS_ADDR_LINKLOCAL(&sin->sin6_addr) &&
	    *(u_short *)&sin->sin6_addr.s6_addr[2] != 0) {
		u_short index;

		index = *(u_short *)&sin->sin6_addr.s6_addr[2];
		*(u_short *)&sin->sin6_addr.s6_addr[2] = 0;
		if (sin->sin6_scope_id == 0)
			sin->sin6_scope_id = ntohs(index);
	}
	scopeid = sin->sin6_scope_id;

	error = getnameinfo((struct sockaddr *)sin, sin->sin6_len, addr_buf,
			    sizeof(addr_buf), NULL, 0, NI_NUMERICHOST);
	if (error != 0)
		inet_ntop(AF_INET6, &sin->sin6_addr, addr_buf,
			  sizeof(addr_buf));
	printf("\tinet6 %s ", addr_buf);

	if (flags & IFF_POINTOPOINT) {
		/* note RTAX_BRD overlap with IFF_BROADCAST */
		sin = (struct sockaddr_in6 *)info->rti_info[RTAX_BRD];
		/*
		 * some of the interfaces do not have valid destination
		 * address.
		 */
		if (sin && sin->sin6_family == AF_INET6) {
			int error;

			/* XXX: embedded link local addr check */
			if (IN6_IS_ADDR_LINKLOCAL(&sin->sin6_addr) &&
			    *(u_short *)&sin->sin6_addr.s6_addr[2] != 0) {
				u_short index;

				index = *(u_short *)&sin->sin6_addr.s6_addr[2];
				*(u_short *)&sin->sin6_addr.s6_addr[2] = 0;
				if (sin->sin6_scope_id == 0)
					sin->sin6_scope_id = ntohs(index);
			}

			error = getnameinfo((struct sockaddr *)sin,
					    sin->sin6_len, addr_buf,
					    sizeof(addr_buf), NULL, 0,
					    NI_NUMERICHOST);
			if (error != 0)
				inet_ntop(AF_INET6, &sin->sin6_addr, addr_buf,
					  sizeof(addr_buf));
			printf("--> %s ", addr_buf);
		}
	}

	sin = (struct sockaddr_in6 *)info->rti_info[RTAX_NETMASK];
	if (!sin)
		sin = &null_sin;
	printf("prefixlen %d ", prefix(&sin->sin6_addr,
		sizeof(struct in6_addr)));

	if ((flags6 & IN6_IFF_ANYCAST) != 0)
		printf("anycast ");
	if ((flags6 & IN6_IFF_TENTATIVE) != 0)
		printf("tentative ");
	if ((flags6 & IN6_IFF_DUPLICATED) != 0)
		printf("duplicated ");
	if ((flags6 & IN6_IFF_DETACHED) != 0)
		printf("detached ");
	if ((flags6 & IN6_IFF_DEPRECATED) != 0)
		printf("deprecated ");
	if ((flags6 & IN6_IFF_AUTOCONF) != 0)
		printf("autoconf ");
	if ((flags6 & IN6_IFF_TEMPORARY) != 0)
		printf("temporary ");

        if (scopeid)
		printf("scopeid 0x%x ", scopeid);

	if (ip6lifetime && (lifetime.ia6t_preferred || lifetime.ia6t_expire)) {
		printf("pltime ");
		if (lifetime.ia6t_preferred) {
			printf("%s ", lifetime.ia6t_preferred < t
				? "0" : sec2str(lifetime.ia6t_preferred - t));
		} else
			printf("infty ");

		printf("vltime ");
		if (lifetime.ia6t_expire) {
			printf("%s ", lifetime.ia6t_expire < t
				? "0" : sec2str(lifetime.ia6t_expire - t));
		} else
			printf("infty ");
	}

	putchar('\n');
}
#endif /*INET6*/

#ifndef NO_IPX
void
ipx_status(int s __unused, struct rt_addrinfo * info)
{
	struct sockaddr_ipx *sipx, null_sipx;

	memset(&null_sipx, 0, sizeof(null_sipx));

	sipx = (struct sockaddr_ipx *)info->rti_info[RTAX_IFA];
	printf("\tipx %s ", ipx_ntoa(sipx->sipx_addr));

	if (flags & IFF_POINTOPOINT) {
		sipx = (struct sockaddr_ipx *)info->rti_info[RTAX_BRD];
		if (!sipx)
			sipx = &null_sipx;
		printf("--> %s ", ipx_ntoa(sipx->sipx_addr));
	}
	putchar('\n');
}
#endif

void
at_status(int s __unused, struct rt_addrinfo * info)
{
	struct sockaddr_at *sat, null_sat;
	struct netrange *nr;

	memset(&null_sat, 0, sizeof(null_sat));

	sat = (struct sockaddr_at *)info->rti_info[RTAX_IFA];
	nr = &sat->sat_range.r_netrange;
	printf("\tatalk %d.%d range %d-%d phase %d",
		ntohs(sat->sat_addr.s_net), sat->sat_addr.s_node,
		ntohs(nr->nr_firstnet), ntohs(nr->nr_lastnet), nr->nr_phase);
	if (flags & IFF_POINTOPOINT) {
		/* note RTAX_BRD overlap with IFF_BROADCAST */
		sat = (struct sockaddr_at *)info->rti_info[RTAX_BRD];
		if (!sat)
			sat = &null_sat;
		printf("--> %d.%d",
			ntohs(sat->sat_addr.s_net), sat->sat_addr.s_node);
	}
	if (flags & IFF_BROADCAST) {
		/* note RTAX_BRD overlap with IFF_POINTOPOINT */
		sat = (struct sockaddr_at *)info->rti_info[RTAX_BRD];
		if (sat)
			printf(" broadcast %d.%d",
				ntohs(sat->sat_addr.s_net),
				sat->sat_addr.s_node);
	}

	putchar('\n');
}

void
link_status(int s __unused, struct rt_addrinfo *info)
{
	struct sockaddr_dl *sdl = (struct sockaddr_dl *)info;

	if (sdl->sdl_alen > 0) {
		if (sdl->sdl_type == IFT_ETHER &&
		    sdl->sdl_alen == ETHER_ADDR_LEN)
			printf("\tether %s\n",
			    ether_ntoa((struct ether_addr *)LLADDR(sdl)));
		else {
			int n = sdl->sdl_nlen > 0 ? sdl->sdl_nlen + 1 : 0;

			printf("\tlladdr %s\n", link_ntoa(sdl) + n);
		}
	}
}

void
Perror(const char *cmd)
{
	switch (errno) {

	case ENXIO:
		errx(1, "%s: no such interface", cmd);
		break;

	case EPERM:
		errx(1, "%s: permission denied", cmd);
		break;

	default:
		err(1, "%s", cmd);
	}
}

#define SIN(x) ((struct sockaddr_in *) &(x))
struct sockaddr_in *sintab[] = {
SIN(ridreq.ifr_addr), SIN(addreq.ifra_addr),
SIN(addreq.ifra_mask), SIN(addreq.ifra_broadaddr)};

void
in_getaddr(const char *s, int which)
{
	struct sockaddr_in *sin = sintab[which];
	struct hostent *hp;
	struct netent *np;

	sin->sin_len = sizeof(*sin);
	if (which != MASK)
		sin->sin_family = AF_INET;

	if (which == ADDR) {
		char *p = NULL;

		if((p = strrchr(s, '/')) != NULL) {
			/* address is `name/masklen' */
			int masklen;
			int ret;
			struct sockaddr_in *min = sintab[MASK];
			*p = '\0';
			ret = sscanf(p+1, "%u", &masklen);
			if(ret != 1 || (masklen < 0 || masklen > 32)) {
				*p = '/';
				errx(1, "%s: bad value", s);
			}
			min->sin_len = sizeof(*min);
			min->sin_addr.s_addr = htonl(~((1LL << (32 - masklen)) - 1) & 
				              0xffffffff);
		}
	}

	if (inet_aton(s, &sin->sin_addr))
		return;
	if ((hp = gethostbyname(s)) != 0)
		bcopy(hp->h_addr, (char *)&sin->sin_addr, 
		    MIN(hp->h_length, sizeof(sin->sin_addr)));
	else if ((np = getnetbyname(s)) != 0)
		sin->sin_addr = inet_makeaddr(np->n_net, INADDR_ANY);
	else
		errx(1, "%s: bad value", s);
}

#ifdef INET6
#define	SIN6(x) ((struct sockaddr_in6 *) &(x))
struct	sockaddr_in6 *sin6tab[] = {
SIN6(in6_ridreq.ifr_addr), SIN6(in6_addreq.ifra_addr),
SIN6(in6_addreq.ifra_prefixmask), SIN6(in6_addreq.ifra_dstaddr)};

void
in6_getaddr(const char *s, int which)
{
	struct sockaddr_in6 *sin = sin6tab[which];
	struct addrinfo hints, *res;
	int error = -1;

	newaddr &= 1;

	sin->sin6_len = sizeof(*sin);
	if (which != MASK)
		sin->sin6_family = AF_INET6;

	if (which == ADDR) {
		char *p = NULL;
		if((p = strrchr(s, '/')) != NULL) {
			*p = '\0';
			in6_getprefix(p + 1, MASK);
			explicit_prefix = 1;
		}
	}

	if (sin->sin6_family == AF_INET6) {
		bzero(&hints, sizeof(struct addrinfo));
		hints.ai_family = AF_INET6;
		error = getaddrinfo(s, NULL, &hints, &res);
	}
	if (error != 0) {
		if (inet_pton(AF_INET6, s, &sin->sin6_addr) != 1)
			errx(1, "%s: bad value", s);
	} else
		bcopy(res->ai_addr, sin, res->ai_addrlen);
}

void
in6_getprefix(const char *plen, int which)
{
	struct sockaddr_in6 *sin = sin6tab[which];
	u_char *cp;
	int len = atoi(plen);

	if ((len < 0) || (len > 128))
		errx(1, "%s: bad value", plen);
	sin->sin6_len = sizeof(*sin);
	if (which != MASK)
		sin->sin6_family = AF_INET6;
	if ((len == 0) || (len == 128)) {
		memset(&sin->sin6_addr, 0xff, sizeof(struct in6_addr));
		return;
	}
	memset((void *)&sin->sin6_addr, 0x00, sizeof(sin->sin6_addr));
	for (cp = (u_char *)&sin->sin6_addr; len > 7; len -= 8)
		*cp++ = 0xff;
	*cp = 0xff << (8 - len);
}
#endif

/*
 * Print a value a la the %b format of the kernel's printf
 */
void
printb(const char *s, unsigned v, const char *bits)
{
	int i, any = 0;
	char c;

	if (bits && *bits == 8)
		printf("%s=%o", s, v);
	else
		printf("%s=%x", s, v);
	bits++;
	if (bits) {
		putchar('<');
		while ((i = *bits++) != '\0') {
			if (v & (1 << (i-1))) {
				if (any)
					putchar(',');
				any = 1;
				for (; (c = *bits) > 32; bits++)
					putchar(c);
			} else
				for (; *bits > 32; bits++)
					;
		}
		putchar('>');
	}
}

#ifndef NO_IPX
#define SIPX(x) ((struct sockaddr_ipx *) &(x))
struct sockaddr_ipx *sipxtab[] = {
SIPX(ridreq.ifr_addr), SIPX(addreq.ifra_addr),
SIPX(addreq.ifra_mask), SIPX(addreq.ifra_broadaddr)};

void
ipx_getaddr(const char *addr, int which)
{
	struct sockaddr_ipx *sipx = sipxtab[which];

	sipx->sipx_family = AF_IPX;
	sipx->sipx_len = sizeof(*sipx);
	sipx->sipx_addr = ipx_addr(addr);
	if (which == MASK)
		printf("Attempt to set IPX netmask will be ineffectual\n");
}
#endif

void
at_getaddr(const char *addr, int which)
{
	struct sockaddr_at *sat = (struct sockaddr_at *) &addreq.ifra_addr;
	u_int net, node;

	sat->sat_family = AF_APPLETALK;
	sat->sat_len = sizeof(*sat);
	if (which == MASK)
		errx(1, "AppleTalk does not use netmasks");
	if (sscanf(addr, "%u.%u", &net, &node) != 2
	    || net > 0xffff || node > 0xfe)
		errx(1, "%s: illegal address", addr);
	sat->sat_addr.s_net = htons(net);
	sat->sat_addr.s_node = node;
}

void
link_getaddr(const char *addr, int which)
{
	char *temp;
	struct sockaddr_dl sdl;
	struct sockaddr *sa = &ridreq.ifr_addr;

	if (which != ADDR)
		errx(1, "can't set link-level netmask or broadcast");
	if ((temp = malloc(strlen(addr) + 1)) == NULL)
		errx(1, "malloc failed");
	temp[0] = ':';
	strcpy(temp + 1, addr);
	sdl.sdl_len = sizeof(sdl);
	link_addr(temp, &sdl);
	free(temp);
	if (sdl.sdl_alen > sizeof(sa->sa_data))
		errx(1, "malformed link-level address");
	sa->sa_family = AF_LINK;
	sa->sa_len = sdl.sdl_alen;
	bcopy(LLADDR(&sdl), sa->sa_data, sdl.sdl_alen);
}

/* XXX  FIXME -- should use strtoul for better parsing. */
void
setatrange(const char *range, int dummy __unused, int s, 
    const struct afswtch *afp)
{
	u_int	first = 123, last = 123;

	if (sscanf(range, "%u-%u", &first, &last) != 2
	    || first == 0 || first > 0xffff
	    || last == 0 || last > 0xffff || first > last)
		errx(1, "%s: illegal net range: %u-%u", range, first, last);
	at_nr.nr_firstnet = htons(first);
	at_nr.nr_lastnet = htons(last);
}

void
setatphase(const char *phase, int dummy __unused, int s, 
    const struct afswtch *afp)
{
	if (!strcmp(phase, "1"))
		at_nr.nr_phase = 1;
	else if (!strcmp(phase, "2"))
		at_nr.nr_phase = 2;
	else
		errx(1, "%s: illegal phase", phase);
}

void
checkatrange(struct sockaddr_at *sat)
{
	if (at_nr.nr_phase == 0)
		at_nr.nr_phase = 2;	/* Default phase 2 */
	if (at_nr.nr_firstnet == 0)
		at_nr.nr_firstnet =	/* Default range of one */
		at_nr.nr_lastnet = sat->sat_addr.s_net;
printf("\tatalk %d.%d range %d-%d phase %d\n",
	ntohs(sat->sat_addr.s_net), sat->sat_addr.s_node,
	ntohs(at_nr.nr_firstnet), ntohs(at_nr.nr_lastnet), at_nr.nr_phase);
	if ((u_short) ntohs(at_nr.nr_firstnet) >
			(u_short) ntohs(sat->sat_addr.s_net)
		    || (u_short) ntohs(at_nr.nr_lastnet) <
			(u_short) ntohs(sat->sat_addr.s_net))
		errx(1, "AppleTalk address is not in range");
	sat->sat_range.r_netrange = at_nr;
}

#ifdef INET6
int
prefix(void *val, int size)
{
        u_char *name = (u_char *)val;
        int byte, bit, plen = 0;

        for (byte = 0; byte < size; byte++, plen += 8)
                if (name[byte] != 0xff)
                        break;
	if (byte == size)
		return (plen);
	for (bit = 7; bit != 0; bit--, plen++)
                if (!(name[byte] & (1 << bit)))
                        break;
        for (; bit != 0; bit--)
                if (name[byte] & (1 << bit))
                        return(0);
        byte++;
        for (; byte < size; byte++)
                if (name[byte])
                        return(0);
        return (plen);
}

static char *
sec2str(time_t total)
{
	static char result[256];
	int days, hours, mins, secs;
	int first = 1;
	char *p = result;

	if (0) {
		days = total / 3600 / 24;
		hours = (total / 3600) % 24;
		mins = (total / 60) % 60;
		secs = total % 60;

		if (days) {
			first = 0;
			p += sprintf(p, "%dd", days);
		}
		if (!first || hours) {
			first = 0;
			p += sprintf(p, "%dh", hours);
		}
		if (!first || mins) {
			first = 0;
			p += sprintf(p, "%dm", mins);
		}
		sprintf(p, "%ds", secs);
	} else
		sprintf(result, "%lu", (unsigned long)total);

	return(result);
}
#endif /*INET6*/

void
ifmaybeload(char *name)
{
	struct module_stat mstat;
	int fileid, modid;
	char ifkind[35], *cp, *dp;

	/* turn interface and unit into module name */
	strcpy(ifkind, "if_");
	for (cp = name, dp = ifkind + 3;
	    (*cp != 0) && !isdigit(*cp); cp++, dp++)
		*dp = *cp;
	*dp = 0;

	/* scan files in kernel */
	mstat.version = sizeof(struct module_stat);
	for (fileid = kldnext(0); fileid > 0; fileid = kldnext(fileid)) {
		/* scan modules in file */
		for (modid = kldfirstmod(fileid); modid > 0;
		     modid = modfnext(modid)) {
			if (modstat(modid, &mstat) < 0)
				continue;
			/* strip bus name if present */
			if ((cp = strchr(mstat.name, '/')) != NULL) {
				cp++;
			} else {
				cp = mstat.name;
			}
			/* already loaded? */
			if (strncmp(name, cp, strlen(cp)) == 0 ||
			    strncmp(ifkind, cp, strlen(cp)) == 0)
				return;
		}
	}

	/* not present, we should try to load it */
	kldload(ifkind);
}

void
list_cloners(void)
{
	struct if_clonereq ifcr;
	char *cp, *buf;
	int idx;
	int s;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		err(1, "socket");

	memset(&ifcr, 0, sizeof(ifcr));

	if (ioctl(s, SIOCIFGCLONERS, &ifcr) < 0)
		err(1, "SIOCIFGCLONERS for count");

	buf = malloc(ifcr.ifcr_total * IFNAMSIZ);
	if (buf == NULL)
		err(1, "unable to allocate cloner name buffer");

	ifcr.ifcr_count = ifcr.ifcr_total;
	ifcr.ifcr_buffer = buf;

	if (ioctl(s, SIOCIFGCLONERS, &ifcr) < 0)
		err(1, "SIOCIFGCLONERS for names");

	/*
	 * In case some disappeared in the mean time, clamp it down.
	 */
	if (ifcr.ifcr_count > ifcr.ifcr_total)
		ifcr.ifcr_count = ifcr.ifcr_total;

	for (cp = buf, idx = 0; idx < ifcr.ifcr_count; idx++, cp += IFNAMSIZ) {
		if (idx > 0)
			putchar(' ');
		printf("%s", cp);
	}

	putchar('\n');
	free(buf);
}

void
clone_create(void)
{
	int s;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		err(1, "socket");

	memset(&ifr, 0, sizeof(ifr));
	(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCIFCREATE, &ifr) < 0)
		err(1, "SIOCIFCREATE");

	/*
	 * If we get a different name back then we put in, we probably
	 * want to print it out, but we might change our mind later so
	 * we just signal our intrest and leave the printout for later.
	 */
	if (strcmp(name, ifr.ifr_name) != 0) {
		printname = 1;
		strlcpy(name, ifr.ifr_name, sizeof(name));
	}

	close(s);
}

void
clone_destroy(const char *val, int d, int s, const struct afswtch *rafp)
{

	(void) strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCIFDESTROY, &ifr) < 0)
		err(1, "SIOCIFDESTROY");
	/*
	 * If we create and destroy an interface in the same command,
	 * there isn't any reason to print it's name.
	 */
	printname = 0;
}
