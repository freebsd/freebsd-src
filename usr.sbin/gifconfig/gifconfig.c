/*	$FreeBSD$	*/
/*	$KAME: gifconfig.c,v 1.14 2001/01/01 04:04:56 jinmei Exp $	*/

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

/*
 * gifconfig, derived from ifconfig
 *
 * @(#) Copyright (c) 1983, 1993\n\
 *	The Regents of the University of California.  All rights reserved.\n
 *
 * @(#)ifconfig.c	8.2 (Berkeley) 2/16/94
 */

/*
 *  951109 - Andrew@pubnix.net - Changed to iterative buffer growing mechanism
 *				 for ifconfig -a so all interfaces are queried.
 *
 *  960101 - peter@freebsd.org - Blow away the SIOCGIFCONF code and use
 *				 sysctl() to get the structured interface conf
 *				 and parse the messages in there. REALLY UGLY!
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>

#include <net/if.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <net/if_var.h>
#endif /* __FreeBSD__ >= 3 */
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/protosw.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <nlist.h>
#include <kvm.h>
#include <fcntl.h>

struct	ifreq		ifr;
struct	ifaliasreq	addreq;
#ifdef INET6
struct	in6_ifreq	in6_ifr;
struct	in6_aliasreq	in6_addreq;
#endif

char	name[32];
int	flags;
int	metric;
int	mtu;
int	setpsrc = 0;
int	newaddr = 0;
int	s;
kvm_t	*kvmd;

#ifdef INET6
char ntop_buf[INET6_ADDRSTRLEN];	/*inet_ntop()*/
#endif

void setifpsrc __P((char *, int));
void setifpdst __P((char *, int));
void setifflags __P((char *, int));
#ifdef SIOCDIFPHYADDR
void delifaddrs __P((char *, int));
#endif

#define	NEXTARG		0xffffff

static struct	cmd {
	char	*c_name;
	int	c_parameter;		/* NEXTARG means next argv */
	void	(*c_func) __P((char *, int));
} cmds[] = {
	{ "up",		IFF_UP,		setifflags } ,
	{ "down",	-IFF_UP,	setifflags },
#ifdef SIOCDIFPHYADDR
	{ "delete",	0,		delifaddrs },
#endif
	{ 0,		0,		setifpsrc },
	{ 0,		0,		setifpdst },
};

/*
 * XNS support liberally adapted from code written at the University of
 * Maryland principally by James O'Toole and Chris Torek.
 */
int main __P((int, char *[]));
void status __P((void));
void phys_status __P((int));
void in_status __P((int));
#ifdef INET6
void in6_status __P((int));
#endif
void ether_status __P((int));
void Perror __P((char *));
void in_getaddr __P((char *, int));
#ifdef INET6
void in6_getaddr __P((char *, int));
void in6_getprefix __P((char *, int));
#endif
void printb __P((char *, unsigned int, char *));
int prefix __P((void *, int));

char ntop_buf[INET6_ADDRSTRLEN];

/* Known address families */
struct afswtch {
	char *af_name;
	short af_af;
	void (*af_status) __P((int));
	void (*af_getaddr) __P((char *, int));
	void (*af_getprefix) __P((char *, int));
	u_long af_pifaddr;
	caddr_t af_addreq;
	caddr_t af_req;
} afs[] = {
#define C(x) ((caddr_t) &x)
	{ "inet", AF_INET, in_status, in_getaddr, 0,
	     SIOCSIFPHYADDR, C(addreq), C(ifr) },
#ifdef INET6
	{ "inet6", AF_INET6, in6_status, in6_getaddr, in6_getprefix,
	     SIOCSIFPHYADDR_IN6, C(in6_addreq), C(in6_ifr) },
#endif
	{ "ether", AF_INET, ether_status, NULL, NULL },	/* XXX not real!! */
	{ 0,	0,	    0,		0,	0 }
};

struct afswtch *afp = NULL;	/*the address family being set or asked about*/

void	rt_xaddrs __P((caddr_t, caddr_t, struct rt_addrinfo *));
int	ifconfig __P((int argc, char *argv[], int af, struct afswtch *rafp));



/*
 * Expand the compacted form of addresses as returned via the
 * configuration read via sysctl().
 */

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

void
rt_xaddrs(cp, cplim, rtinfo)
	caddr_t cp, cplim;
	struct rt_addrinfo *rtinfo;
{
	struct sockaddr *sa;
	int i;

	memset(rtinfo->rti_info, 0, sizeof(rtinfo->rti_info));
	for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
		if ((rtinfo->rti_addrs & (1 << i)) == 0)
			continue;
		rtinfo->rti_info[i] = sa = (struct sockaddr *)cp;
		ADVANCE(cp, sa);
	}
}


/*
 * Grunge for new-style sysctl() decoding.. :-(
 * Apologies to the world for committing gross things like this in 1996..
 */
struct if_msghdr *ifm;
struct ifa_msghdr *ifam;
struct sockaddr_dl *sdl;
struct rt_addrinfo info;
char *buf, *lim, *next;


int
main(argc, argv)
	int argc;
	char *argv[];
{
	int af = AF_INET;
	struct afswtch *rafp = NULL;
	size_t needed;
	int mib[6];
	int all;

	if (argc < 2) {
		fprintf(stderr,
		    "usage: gifconfig interface [af] [physsrc physdst]\n");
#ifdef SIOCDIFPHYADDR
		fprintf(stderr,
		    "       gifconfig interface delete\n");
#endif
		fprintf(stderr,
		    "       gifconfig -a\n");
		exit(1);
	}
	argc--, argv++;
	strncpy(name, *argv, sizeof(name));
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	argc--, argv++;
	if (argc > 0) {
		for (afp = rafp = afs; rafp->af_name; rafp++)
			if (strcmp(rafp->af_name, *argv) == 0) {
				afp = rafp; argc--; argv++;
				break;
			}
		rafp = afp;
		af = ifr.ifr_addr.sa_family = rafp->af_af;
	}

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;	/* address family */
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;

	/* if particular family specified, only ask about it */
	if (afp) {
		mib[3] = afp->af_af;
	}

	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		errx(1, "iflist-sysctl-estimate");
	if ((buf = malloc(needed)) == NULL)
		errx(1, "malloc");
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		errx(1, "actual retrieval of interface table");
	lim = buf + needed;

	all = 0;
	if (strcmp(name, "-a") == 0)
		all = 1;	/* All interfaces */
	else if (strcmp(name, "-au") == 0)
		all = 2;	/* All IFF_UPinterfaces */
	else if (strcmp(name, "-ad") == 0)
		all = 3;	/* All !IFF_UP interfaces */

	for (next = buf; next < lim; next += ifm->ifm_msglen) {

		ifm = (struct if_msghdr *)next;

		/* XXX: Swallow up leftover NEWADDR messages */
		if (ifm->ifm_type == RTM_NEWADDR)
			continue;

		if (ifm->ifm_type == RTM_IFINFO) {
			sdl = (struct sockaddr_dl *)(ifm + 1);
			flags = ifm->ifm_flags;
		} else {
			errx(1, "out of sync parsing NET_RT_IFLIST");
		}

		switch(all) {
		case -1:
		case 0:
			if (strlen(name) != sdl->sdl_nlen)
				continue; /* not same len */
			if (strncmp(name, sdl->sdl_data, sdl->sdl_nlen) != 0)
				continue; /* not same name */
			break;
		case 1:
			break;	/* always do it */
		case 2:
			if ((flags & IFF_UP) == 0)
				continue; /* not up */
			break;
		case 3:
			if (flags & IFF_UP)
				continue; /* not down */
			break;
		}

		/*
		 * Let's just do it for gif only
		 */
		if (sdl->sdl_type != IFT_GIF) {
			if (all != 0)
				continue;

			fprintf(stderr, "gifconfig: %s is not gif.\n",
				ifr.ifr_name);
			exit(1);
		}

		if (all > 0) {
			strncpy(name, sdl->sdl_data, sdl->sdl_nlen);
			name[sdl->sdl_nlen] = '\0';
		}

		if ((s = socket(af, SOCK_DGRAM, 0)) < 0) {
			perror("gifconfig: socket");
			exit(1);
		}

		ifconfig(argc,argv,af,rafp);

		close(s);

		if (all == 0) {
			all = -1; /* flag it as 'done' */
			break;
		}
	}
	free(buf);

	if (all == 0)
		errx(1, "interface %s does not exist", name);
	

	exit (0);
}


int
ifconfig(argc, argv, af, rafp)
	int argc;
	char *argv[];
	int af;
	struct afswtch *rafp;
{

	af = 0;		/*fool gcc*/

	strncpy(ifr.ifr_name, name, sizeof ifr.ifr_name);
#ifdef INET6
	strncpy(in6_ifr.ifr_name, name, sizeof in6_ifr.ifr_name);
#endif /* INET6 */

	if (ioctl(s, SIOCGIFMETRIC, (caddr_t)&ifr) < 0)
		perror("ioctl (SIOCGIFMETRIC)");
	else
		metric = ifr.ifr_metric;

#if defined(SIOCGIFMTU) && !defined(__OpenBSD__)
	if (ioctl(s, SIOCGIFMTU, (caddr_t)&ifr) < 0)
		perror("ioctl (SIOCGIFMTU)");
	else
		mtu = ifr.ifr_mtu;
#else
	mtu = 0;
#endif

	if (argc == 0) {
		status();
		return(0);
	}

	while (argc > 0) {
		register struct cmd *p;

		for (p = cmds; p->c_name; p++)
			if (strcmp(*argv, p->c_name) == 0)
				break;
		if (p->c_name == 0 && setpsrc)
			p++;	/* got src, do dst */
		if (p->c_func) {
			if (p->c_parameter == NEXTARG) {
				if (argv[1] == NULL)
					errx(1, "'%s' requires argument",
					    p->c_name);
				(*p->c_func)(argv[1], 0);
				argc--, argv++;
			} else
				(*p->c_func)(*argv, p->c_parameter);
		}
		argc--, argv++;
	}
	if (newaddr) {
		strncpy(rafp->af_addreq, name, sizeof ifr.ifr_name);
		if (ioctl(s, rafp->af_pifaddr, rafp->af_addreq) < 0)
			Perror("ioctl (SIOCSIFPHYADDR)");
	}
	else if (setpsrc) {
		errx(1, "destination is not specified");
	}
	return(0);
}
#define PSRC	0
#define PDST	1

/*ARGSUSED*/
void
setifpsrc(addr, param)
	char *addr;
	int param;
{
	param = 0;	/*fool gcc*/
	(*afp->af_getaddr)(addr, PSRC);
	setpsrc = 1;
}

/*ARGSUSED*/
void
setifpdst(addr, param)
	char *addr;
	int param;
{
	param = 0;	/*fool gcc*/
	(*afp->af_getaddr)(addr, PDST);
	newaddr = 1;
}

void
setifflags(vname, value)
	char *vname;
	int value;
{
 	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
 		Perror("ioctl (SIOCGIFFLAGS)");
 		exit(1);
 	}
	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
 	flags = ifr.ifr_flags;

	if (value < 0) {
		value = -value;
		flags &= ~value;
	} else
		flags |= value;
	ifr.ifr_flags = flags;
	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) < 0)
		Perror(vname);
}

#ifdef SIOCDIFPHYADDR
/* ARGSUSED */
void
delifaddrs(vname, param)
	char *vname;
	int param;
{
	param = 0;		/* fool gcc */
	vname = NULL;		/* ditto */

	if (ioctl(s, SIOCDIFPHYADDR, (caddr_t)&ifr) < 0)
		err(1, "ioctl(SIOCDIFPHYADDR)");
}
#endif

#define	IFFBITS \
"\020\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5POINTOPOINT\6NOTRAILERS\7RUNNING\10NOARP\
\11PROMISC\12ALLMULTI\13OACTIVE\14SIMPLEX\15LINK0\16LINK1\17LINK2\20MULTICAST"

/*
 * Print the status of the interface.  If an address family was
 * specified, show it and it only; otherwise, show them all.
 */
void
status()
{
	struct afswtch *p = NULL;
	char *mynext;
	struct if_msghdr *myifm;

	printf("%s: ", name);
	printb("flags", flags, IFFBITS);
	if (metric)
		printf(" metric %d", metric);
	if (mtu)
		printf(" mtu %d", mtu);
	putchar('\n');

	/*
	 * XXX: Sigh. This is bad, I know.  At this point, we may have
	 * *zero* RTM_NEWADDR's, so we have to "feel the water" before
	 * incrementing the loop.  One day, I might feel inspired enough
	 * to get the top level loop to pass a count down here so we
	 * dont have to mess with this.  -Peter
	 */
	myifm = ifm;

	while (1) {

		mynext = next + ifm->ifm_msglen;

		if (mynext >= lim)
			break;

		myifm = (struct if_msghdr *)mynext;

		if (myifm->ifm_type != RTM_NEWADDR)
			break;

		next = mynext;

		ifm = (struct if_msghdr *)next;

		ifam = (struct ifa_msghdr *)myifm;
		info.rti_addrs = ifam->ifam_addrs;

		/* Expand the compacted addresses */
		rt_xaddrs((char *)(ifam + 1), ifam->ifam_msglen + (char *)ifam,
			  &info);

		if (afp) {
			if (afp->af_af == info.rti_info[RTAX_IFA]->sa_family &&
			    afp->af_status != ether_status) {
				p = afp;
				if (p->af_status != ether_status)
					(*p->af_status)(1);
			}
		} else for (p = afs; p->af_name; p++) {
			if (p->af_af == info.rti_info[RTAX_IFA]->sa_family &&
			    p->af_status != ether_status)
				(*p->af_status)(0);
		}
	}
	if (afp == NULL || afp->af_status == ether_status)
		ether_status(0);
	else if (afp && !p) {
		warnx("%s has no %s IFA address!", name, afp->af_name);
	}

	phys_status(0);
}

void
phys_status(force)
	int force;
{
	char psrcaddr[256];
	char pdstaddr[256];
	char hostname[NI_MAXHOST];
	int flags = NI_NUMERICHOST;
	char *af;
#ifndef SIOCGLIFPHYADDR
	u_long srccmd, dstcmd;
	struct ifreq *ifrp;
#ifdef INET6
	int s6;
#endif

	force = 0;	/*fool gcc*/

	psrcaddr[0] = pdstaddr[0] = '\0';

#ifdef INET6
	s6 = socket(AF_INET6, SOCK_DGRAM, 0);
	if (s6 < 0) {
		ifrp = &ifr;
		srccmd = SIOCGIFPSRCADDR;
		dstcmd = SIOCGIFPDSTADDR;
	} else {
		close(s6);
		srccmd = SIOCGIFPSRCADDR_IN6;
		dstcmd = SIOCGIFPDSTADDR_IN6;
		ifrp = (struct ifreq *)&in6_ifr;
	}
#else /* INET6 */
	ifrp = &ifr;
	srccmd = SIOCGIFPSRCADDR;
	dstcmd = SIOCGIFPDSTADDR;
#endif /* INET6 */

	if (0 <= ioctl(s, srccmd, (caddr_t)ifrp)) {
#ifdef INET6
		if (ifrp->ifr_addr.sa_family == AF_INET6)
			af = "inet6";
		else
			af = "inet";
#else
		af = "inet";
#endif /* INET6 */
		if (getnameinfo(&ifrp->ifr_addr, ifrp->ifr_addr.sa_len,
		    psrcaddr, sizeof(psrcaddr), 0, 0, flags) != 0)
			psrcaddr[0] = '\0';
	}
	if (0 <= ioctl(s, dstcmd, (caddr_t)ifrp)) {
		if (getnameinfo(&ifrp->ifr_addr, ifrp->ifr_addr.sa_len,
		    pdstaddr, sizeof(pdstaddr), 0, 0, flags) != 0)
			pdstaddr[0] = '\0';
	}
	printf("\tphysical address %s %s --> %s\n", af, psrcaddr, pdstaddr);
#else
	struct if_laddrreq iflr;

	force = 0;	/*fool gcc*/

	psrcaddr[0] = pdstaddr[0] = '\0';

	memset(&iflr, 0, sizeof(iflr));
	memcpy(iflr.iflr_name, ifr.ifr_name, sizeof(iflr.iflr_name));

	if (0 <= ioctl(s, SIOCGLIFPHYADDR, (caddr_t)&iflr)) {
		switch (iflr.addr.ss_family) {
		case AF_INET:
			af = "inet";
			break;
#ifdef INET6
		case AF_INET6:
			af = "inet6";
			break;
#endif /* INET6 */
		}
		if (getnameinfo((struct sockaddr *)&iflr.addr, iflr.addr.ss_len,
		    psrcaddr, sizeof(psrcaddr), 0, 0, flags) != 0)
			psrcaddr[0] = '\0';
		if (getnameinfo((struct sockaddr *)&iflr.dstaddr,
		    iflr.dstaddr.ss_len, pdstaddr, sizeof(pdstaddr), 0, 0,
		    flags) != 0)
			pdstaddr[0] = '\0';
	}
	printf("\tphysical address %s %s --> %s\n", af, psrcaddr, pdstaddr);
#endif
}

void
in_status(force)
	int force;
{
	struct sockaddr_in *sin, null_sin;
#if 0
	char *inet_ntoa();
#endif
	
	memset(&null_sin, 0, sizeof(null_sin));

	sin = (struct sockaddr_in *)info.rti_info[RTAX_IFA];
	if (!sin || sin->sin_family != AF_INET) {
		if (!force)
			return;
		/* warnx("%s has no AF_INET IFA address!", name); */
		sin = &null_sin;
	}
	printf("\tinet %s ", inet_ntoa(sin->sin_addr));

	if (flags & IFF_POINTOPOINT) {
		/* note RTAX_BRD overlap with IFF_BROADCAST */
		sin = (struct sockaddr_in *)info.rti_info[RTAX_BRD];
		if (!sin)
			sin = &null_sin;
		printf("--> %s ", inet_ntoa(sin->sin_addr));
	}

	sin = (struct sockaddr_in *)info.rti_info[RTAX_NETMASK];
	if (!sin)
		sin = &null_sin;
	printf("netmask 0x%x ", (u_int32_t)ntohl(sin->sin_addr.s_addr));

	if (flags & IFF_BROADCAST) {
		/* note RTAX_BRD overlap with IFF_POINTOPOINT */
		sin = (struct sockaddr_in *)info.rti_info[RTAX_BRD];
		if (sin && sin->sin_addr.s_addr != 0)
			printf("broadcast %s", inet_ntoa(sin->sin_addr));
	}
	putchar('\n');
}

#ifdef INET6
void
in6_status(force)
	int force;
{
	struct sockaddr_in6 *sin, null_sin;
	char hostname[NI_MAXHOST];
	int niflags = NI_NUMERICHOST;

	memset(&null_sin, 0, sizeof(null_sin));
#ifdef NI_WITHSCOPEID
	niflags |= NI_WITHSCOPEID;
#endif

	sin = (struct sockaddr_in6 *)info.rti_info[RTAX_IFA];
	if (!sin || sin->sin6_family != AF_INET6) {
		if (!force)
			return;
		/* warnx("%s has no AF_INET6 IFA address!", name); */
		sin = &null_sin;
	}
#ifdef __KAME__
	if (IN6_IS_ADDR_LINKLOCAL(&sin->sin6_addr)) {
		sin->sin6_scope_id =
			ntohs(*(u_int16_t *)&sin->sin6_addr.s6_addr[2]);
		sin->sin6_addr.s6_addr[2] = 0;
		sin->sin6_addr.s6_addr[3] = 0;
	}
#endif
	getnameinfo((struct sockaddr *)sin, sin->sin6_len,
		    hostname, sizeof(hostname), 0, 0, niflags);
	printf("\tinet6 %s ", hostname);

	if (flags & IFF_POINTOPOINT) {
		/* note RTAX_BRD overlap with IFF_BROADCAST */
		sin = (struct sockaddr_in6 *)info.rti_info[RTAX_BRD];
		/*
		 * some of ther interfaces do not have valid destination
		 * address.
		 */
		if (sin->sin6_family == AF_INET6) {
#ifdef __KAME__
			if (IN6_IS_ADDR_LINKLOCAL(&sin->sin6_addr)) {
				sin->sin6_scope_id =
					ntohs(*(u_int16_t *)&sin->sin6_addr.s6_addr[2]);
				sin->sin6_addr.s6_addr[2] = 0;
				sin->sin6_addr.s6_addr[3] = 0;
			}
#endif
			getnameinfo((struct sockaddr *)sin, sin->sin6_len,
				    hostname, sizeof(hostname), 0, 0, niflags);
			printf("--> %s ", hostname);
		}
	}

	sin = (struct sockaddr_in6 *)info.rti_info[RTAX_NETMASK];
	if (!sin)
		sin = &null_sin;
	printf(" prefixlen %d ", prefix(&sin->sin6_addr,
		sizeof(struct in6_addr)));

	putchar('\n');
}
#endif /*INET6*/

/*ARGSUSED*/
void
ether_status(dummy)
	int dummy;
{
	char *cp;
	int n;

	dummy = 0;	/*fool gcc*/

	cp = (char *)LLADDR(sdl);
	if ((n = sdl->sdl_alen) > 0) {
		if (sdl->sdl_type == IFT_ETHER)
			printf ("\tether ");
		else
			printf ("\tlladdr ");
             	while (--n >= 0)
			printf("%02x%c",*cp++ & 0xff, n>0? ':' : ' ');
		putchar('\n');
	}
}

void
Perror(cmd)
	char *cmd;
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
SIN(addreq.ifra_addr), SIN(addreq.ifra_dstaddr)};

void
in_getaddr(s, which)
	char *s;
	int which;
{
	register struct sockaddr_in *sin = sintab[which];
	struct hostent *hp;
	struct netent *np;

	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;

	if (inet_aton(s, &sin->sin_addr))
		;
	else if ((hp = gethostbyname(s)) != NULL)
		bcopy(hp->h_addr, (char *)&sin->sin_addr, hp->h_length);
	else if ((np = getnetbyname(s)) != NULL)
		sin->sin_addr = inet_makeaddr(np->n_net, INADDR_ANY);
	else
		errx(1, "%s: bad value", s);
}

#ifdef INET6
#define SIN6(x) ((struct sockaddr_in6 *) &(x))
struct sockaddr_in6 *sin6tab[] = {
SIN6(in6_addreq.ifra_addr), SIN6(in6_addreq.ifra_dstaddr)};

void
in6_getaddr(s, which)
	char *s;
	int which;
{
	register struct sockaddr_in6 *sin = sin6tab[which];

	sin->sin6_len = sizeof(*sin);
	sin->sin6_family = AF_INET6;

        if (inet_pton(AF_INET6, s, &sin->sin6_addr) != 1)
		errx(1, "%s: bad value", s);
}

void
in6_getprefix(plen, which)
	char *plen;
	int which;
{
	register struct sockaddr_in6 *sin = sin6tab[which];
	register u_char *cp;
	int len = atoi(plen);

	if ((len < 0) || (len > 128))
		errx(1, "%s: bad value", plen);
	sin->sin6_len = sizeof(*sin);
	sin->sin6_family = AF_INET6;
	if ((len == 0) || (len == 128)) {
		memset(&sin->sin6_addr, -1, sizeof(struct in6_addr));
		return;
	}
	for (cp = (u_char *)&sin->sin6_addr; len > 7; len -= 8)
		*cp++ = -1;
	*cp = (-1) << (8 - len);
}
#endif

/*
 * Print a value a la the %b format of the kernel's printf
 */
void
printb(s, v, bits)
	char *s;
	register unsigned int v;
	register char *bits;
{
	register int i, any = 0;
	register char c;

	if (bits && *bits == 8)
		printf("%s=%o", s, v & 0xffff);
	else
		printf("%s=%x", s, v & 0xffff);
	bits++;
	if (bits) {
		putchar('<');
		while ((i = *bits++) != 0) {
			if ((v & (1 << (i-1))) != 0) {
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

#ifdef INET6
int
prefix(val, size)
        void *val;
        int size;
{
        register u_char *name = (u_char *)val;
        register int byte, bit, plen = 0;

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
#endif /*INET6*/
