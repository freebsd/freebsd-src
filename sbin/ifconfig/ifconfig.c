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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)ifconfig.c	8.2 (Berkeley) 2/16/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define	NSIP
#include <netns/ns.h>
#include <netns/ns_if.h>
#include <netdb.h>

#define EON
#include <netiso/iso.h>
#include <netiso/iso_var.h>
#include <sys/protosw.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct	ifreq		ifr, ridreq;
struct	ifaliasreq	addreq;
struct	iso_ifreq	iso_ridreq;
struct	iso_aliasreq	iso_addreq;
struct	sockaddr_in	netmask;

char	name[30];
int	flags;
int	metric;
int	nsellength = 1;
int	setaddr;
int	setipdst;
int	doalias;
int	clearaddr;
int	newaddr = 1;
int	s;
extern	int errno;

int	setifflags(), setifaddr(), setifdstaddr(), setifnetmask();
int	setifmetric(), setifbroadaddr(), setifipdst();
int	notealias(), setsnpaoffset(), setnsellength(), notrailers();

#define	NEXTARG		0xffffff

struct	cmd {
	char	*c_name;
	int	c_parameter;		/* NEXTARG means next argv */
	int	(*c_func)();
} cmds[] = {
	{ "up",		IFF_UP,		setifflags } ,
	{ "down",	-IFF_UP,	setifflags },
	{ "trailers",	-1,		notrailers },
	{ "-trailers",	1,		notrailers },
	{ "arp",	-IFF_NOARP,	setifflags },
	{ "-arp",	IFF_NOARP,	setifflags },
	{ "debug",	IFF_DEBUG,	setifflags },
	{ "-debug",	-IFF_DEBUG,	setifflags },
	{ "alias",	IFF_UP,		notealias },
	{ "-alias",	-IFF_UP,	notealias },
	{ "delete",	-IFF_UP,	notealias },
#ifdef notdef
#define	EN_SWABIPS	0x1000
	{ "swabips",	EN_SWABIPS,	setifflags },
	{ "-swabips",	-EN_SWABIPS,	setifflags },
#endif
	{ "netmask",	NEXTARG,	setifnetmask },
	{ "metric",	NEXTARG,	setifmetric },
	{ "broadcast",	NEXTARG,	setifbroadaddr },
	{ "ipdst",	NEXTARG,	setifipdst },
	{ "snpaoffset",	NEXTARG,	setsnpaoffset },
	{ "nsellength",	NEXTARG,	setnsellength },
	{ "link0",	IFF_LINK0,	setifflags } ,
	{ "-link0",	-IFF_LINK0,	setifflags } ,
	{ "link1",	IFF_LINK1,	setifflags } ,
	{ "-link1",	-IFF_LINK1,	setifflags } ,
	{ "link2",	IFF_LINK2,	setifflags } ,
	{ "-link2",	-IFF_LINK2,	setifflags } ,
	{ 0,		0,		setifaddr },
	{ 0,		0,		setifdstaddr },
};

/*
 * XNS support liberally adapted from code written at the University of
 * Maryland principally by James O'Toole and Chris Torek.
 */
int	in_status(), in_getaddr();
int	xns_status(), xns_getaddr();
int	iso_status(), iso_getaddr();

/* Known address families */
struct afswtch {
	char *af_name;
	short af_af;
	int (*af_status)();
	int (*af_getaddr)();
	int af_difaddr;
	int af_aifaddr;
	caddr_t af_ridreq;
	caddr_t af_addreq;
} afs[] = {
#define C(x) ((caddr_t) &x)
	{ "inet", AF_INET, in_status, in_getaddr,
	     SIOCDIFADDR, SIOCAIFADDR, C(ridreq), C(addreq) },
	{ "ns", AF_NS, xns_status, xns_getaddr,
	     SIOCDIFADDR, SIOCAIFADDR, C(ridreq), C(addreq) },
	{ "iso", AF_ISO, iso_status, iso_getaddr,
	     SIOCDIFADDR_ISO, SIOCAIFADDR_ISO, C(iso_ridreq), C(iso_addreq) },
	{ 0,	0,	    0,		0 }
};

struct afswtch *afp;	/*the address family being set or asked about*/

main(argc, argv)
	int argc;
	char *argv[];
{
	int af = AF_INET;
	register struct afswtch *rafp;

	if (argc < 2) {
		fprintf(stderr, "usage: ifconfig interface\n%s%s%s%s%s",
		    "\t[ af [ address [ dest_addr ] ] [ up ] [ down ]",
			    "[ netmask mask ] ]\n",
		    "\t[ metric n ]\n",
		    "\t[ arp | -arp ]\n",
		    "\t[ link0 | -link0 ] [ link1 | -link1 ] [ link2 | -link2 ] \n");
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
	s = socket(af, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("ifconfig: socket");
		exit(1);
	}
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		Perror("ioctl (SIOCGIFFLAGS)");
		exit(1);
	}
	strncpy(ifr.ifr_name, name, sizeof ifr.ifr_name);
	flags = ifr.ifr_flags;
	if (ioctl(s, SIOCGIFMETRIC, (caddr_t)&ifr) < 0)
		perror("ioctl (SIOCGIFMETRIC)");
	else
		metric = ifr.ifr_metric;
	if (argc == 0) {
		status();
		exit(0);
	}
	while (argc > 0) {
		register struct cmd *p;

		for (p = cmds; p->c_name; p++)
			if (strcmp(*argv, p->c_name) == 0)
				break;
		if (p->c_name == 0 && setaddr)
			p++;	/* got src, do dst */
		if (p->c_func) {
			if (p->c_parameter == NEXTARG) {
				if (argv[1] == NULL)
					errx(1, "'%s' requires argument",
					    p->c_name);
				(*p->c_func)(argv[1]);
				argc--, argv++;
			} else
				(*p->c_func)(*argv, p->c_parameter);
		}
		argc--, argv++;
	}
	if (af == AF_ISO)
		adjust_nsellength();
	if (setipdst && af==AF_NS) {
		struct nsip_req rq;
		int size = sizeof(rq);

		rq.rq_ns = addreq.ifra_addr;
		rq.rq_ip = addreq.ifra_dstaddr;

		if (setsockopt(s, 0, SO_NSIP_ROUTE, &rq, size) < 0)
			Perror("Encapsulation Routing");
	}
	if (clearaddr) {
		int ret;
		strncpy(rafp->af_ridreq, name, sizeof ifr.ifr_name);
		if ((ret = ioctl(s, rafp->af_difaddr, rafp->af_ridreq)) < 0) {
			if (errno == EADDRNOTAVAIL && (doalias >= 0)) {
				/* means no previous address for interface */
			} else
				Perror("ioctl (SIOCDIFADDR)");
		}
	}
	if (newaddr) {
		strncpy(rafp->af_addreq, name, sizeof ifr.ifr_name);
		if (ioctl(s, rafp->af_aifaddr, rafp->af_addreq) < 0)
			Perror("ioctl (SIOCAIFADDR)");
	}
	exit(0);
}
#define RIDADDR 0
#define ADDR	1
#define MASK	2
#define DSTADDR	3

/*ARGSUSED*/
setifaddr(addr, param)
	char *addr;
	short param;
{
	/*
	 * Delay the ioctl to set the interface addr until flags are all set.
	 * The address interpretation may depend on the flags,
	 * and the flags may change when the address is set.
	 */
	setaddr++;
	if (doalias == 0)
		clearaddr = 1;
	(*afp->af_getaddr)(addr, (doalias >= 0 ? ADDR : RIDADDR));
}

setifnetmask(addr)
	char *addr;
{
	(*afp->af_getaddr)(addr, MASK);
}

setifbroadaddr(addr)
	char *addr;
{
	(*afp->af_getaddr)(addr, DSTADDR);
}

setifipdst(addr)
	char *addr;
{
	in_getaddr(addr, DSTADDR);
	setipdst++;
	clearaddr = 0;
	newaddr = 0;
}
#define rqtosa(x) (&(((struct ifreq *)(afp->x))->ifr_addr))
/*ARGSUSED*/
notealias(addr, param)
	char *addr;
{
	if (setaddr && doalias == 0 && param < 0)
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
notrailers(vname, value)
	char *vname;
	int value;
{
	printf("Note: trailers are no longer sent, but always received\n");
}

/*ARGSUSED*/
setifdstaddr(addr, param)
	char *addr;
	int param;
{
	(*afp->af_getaddr)(addr, DSTADDR);
}

setifflags(vname, value)
	char *vname;
	short value;
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

setifmetric(val)
	char *val;
{
	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	ifr.ifr_metric = atoi(val);
	if (ioctl(s, SIOCSIFMETRIC, (caddr_t)&ifr) < 0)
		perror("ioctl (set metric)");
}

setsnpaoffset(val)
	char *val;
{
	iso_addreq.ifra_snpaoffset = atoi(val);
}

#define	IFFBITS \
"\020\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5POINTOPOINT\6NOTRAILERS\7RUNNING\10NOARP\
\11PROMISC\12ALLMULTI\13OACTIVE\14SIMPLEX\15LINK0\16LINK1\17LINK2\20MULTICAST"

/*
 * Print the status of the interface.  If an address family was
 * specified, show it and it only; otherwise, show them all.
 */
status()
{
	register struct afswtch *p = afp;
	short af = ifr.ifr_addr.sa_family;

	printf("%s: ", name);
	printb("flags", flags, IFFBITS);
	if (metric)
		printf(" metric %d", metric);
	putchar('\n');
	if ((p = afp) != NULL) {
		(*p->af_status)(1);
	} else for (p = afs; p->af_name; p++) {
		ifr.ifr_addr.sa_family = p->af_af;
		(*p->af_status)(0);
	}
}

in_status(force)
	int force;
{
	struct sockaddr_in *sin;
	char *inet_ntoa();

	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	if (ioctl(s, SIOCGIFADDR, (caddr_t)&ifr) < 0) {
		if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
			if (!force)
				return;
			bzero((char *)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
		} else
			perror("ioctl (SIOCGIFADDR)");
	}
	sin = (struct sockaddr_in *)&ifr.ifr_addr;
	printf("\tinet %s ", inet_ntoa(sin->sin_addr));
	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	if (ioctl(s, SIOCGIFNETMASK, (caddr_t)&ifr) < 0) {
		if (errno != EADDRNOTAVAIL)
			perror("ioctl (SIOCGIFNETMASK)");
		bzero((char *)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
	} else
		netmask.sin_addr =
		    ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
	if (flags & IFF_POINTOPOINT) {
		if (ioctl(s, SIOCGIFDSTADDR, (caddr_t)&ifr) < 0) {
			if (errno == EADDRNOTAVAIL)
			    bzero((char *)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
			else
			    perror("ioctl (SIOCGIFDSTADDR)");
		}
		strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
		sin = (struct sockaddr_in *)&ifr.ifr_dstaddr;
		printf("--> %s ", inet_ntoa(sin->sin_addr));
	}
	printf("netmask 0x%x ", ntohl(netmask.sin_addr.s_addr));
	if (flags & IFF_BROADCAST) {
		if (ioctl(s, SIOCGIFBRDADDR, (caddr_t)&ifr) < 0) {
			if (errno == EADDRNOTAVAIL)
			    bzero((char *)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
			else
			    perror("ioctl (SIOCGIFADDR)");
		}
		strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
		sin = (struct sockaddr_in *)&ifr.ifr_addr;
		if (sin->sin_addr.s_addr != 0)
			printf("broadcast %s", inet_ntoa(sin->sin_addr));
	}
	putchar('\n');
}


xns_status(force)
	int force;
{
	struct sockaddr_ns *sns;

	close(s);
	s = socket(AF_NS, SOCK_DGRAM, 0);
	if (s < 0) {
		if (errno == EPROTONOSUPPORT)
			return;
		perror("ifconfig: socket");
		exit(1);
	}
	if (ioctl(s, SIOCGIFADDR, (caddr_t)&ifr) < 0) {
		if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
			if (!force)
				return;
			bzero((char *)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
		} else
			perror("ioctl (SIOCGIFADDR)");
	}
	strncpy(ifr.ifr_name, name, sizeof ifr.ifr_name);
	sns = (struct sockaddr_ns *)&ifr.ifr_addr;
	printf("\tns %s ", ns_ntoa(sns->sns_addr));
	if (flags & IFF_POINTOPOINT) { /* by W. Nesheim@Cornell */
		if (ioctl(s, SIOCGIFDSTADDR, (caddr_t)&ifr) < 0) {
			if (errno == EADDRNOTAVAIL)
			    bzero((char *)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
			else
			    Perror("ioctl (SIOCGIFDSTADDR)");
		}
		strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
		sns = (struct sockaddr_ns *)&ifr.ifr_dstaddr;
		printf("--> %s ", ns_ntoa(sns->sns_addr));
	}
	putchar('\n');
}

iso_status(force)
	int force;
{
	struct sockaddr_iso *siso;
	struct iso_ifreq ifr;

	close(s);
	s = socket(AF_ISO, SOCK_DGRAM, 0);
	if (s < 0) {
		if (errno == EPROTONOSUPPORT)
			return;
		perror("ifconfig: socket");
		exit(1);
	}
	bzero((caddr_t)&ifr, sizeof(ifr));
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFADDR_ISO, (caddr_t)&ifr) < 0) {
		if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
			if (!force)
				return;
			bzero((char *)&ifr.ifr_Addr, sizeof(ifr.ifr_Addr));
		} else {
			perror("ioctl (SIOCGIFADDR_ISO)");
			exit(1);
		}
	}
	strncpy(ifr.ifr_name, name, sizeof ifr.ifr_name);
	siso = &ifr.ifr_Addr;
	printf("\tiso %s ", iso_ntoa(&siso->siso_addr));
	if (ioctl(s, SIOCGIFNETMASK_ISO, (caddr_t)&ifr) < 0) {
		if (errno != EADDRNOTAVAIL)
			perror("ioctl (SIOCGIFNETMASK_ISO)");
	} else {
		printf(" netmask %s ", iso_ntoa(&siso->siso_addr));
	}
	if (flags & IFF_POINTOPOINT) {
		if (ioctl(s, SIOCGIFDSTADDR_ISO, (caddr_t)&ifr) < 0) {
			if (errno == EADDRNOTAVAIL)
			    bzero((char *)&ifr.ifr_Addr, sizeof(ifr.ifr_Addr));
			else
			    Perror("ioctl (SIOCGIFDSTADDR_ISO)");
		}
		strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
		siso = &ifr.ifr_Addr;
		printf("--> %s ", iso_ntoa(&siso->siso_addr));
	}
	putchar('\n');
}

Perror(cmd)
	char *cmd;
{
	extern int errno;

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

struct	in_addr inet_makeaddr();

#define SIN(x) ((struct sockaddr_in *) &(x))
struct sockaddr_in *sintab[] = {
SIN(ridreq.ifr_addr), SIN(addreq.ifra_addr),
SIN(addreq.ifra_mask), SIN(addreq.ifra_broadaddr)};

in_getaddr(s, which)
	char *s;
{
	register struct sockaddr_in *sin = sintab[which];
	struct hostent *hp;
	struct netent *np;
	int val;

	sin->sin_len = sizeof(*sin);
	if (which != MASK)
		sin->sin_family = AF_INET;

	if ((val = inet_addr(s)) != -1)
		sin->sin_addr.s_addr = val;
	else if (hp = gethostbyname(s))
		bcopy(hp->h_addr, (char *)&sin->sin_addr, hp->h_length);
	else if (np = getnetbyname(s))
		sin->sin_addr = inet_makeaddr(np->n_net, INADDR_ANY);
	else
		errx(1, "%s: bad value", s);
}

/*
 * Print a value a la the %b format of the kernel's printf
 */
printb(s, v, bits)
	char *s;
	register char *bits;
	register unsigned short v;
{
	register int i, any = 0;
	register char c;

	if (bits && *bits == 8)
		printf("%s=%o", s, v);
	else
		printf("%s=%x", s, v);
	bits++;
	if (bits) {
		putchar('<');
		while (i = *bits++) {
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

#define SNS(x) ((struct sockaddr_ns *) &(x))
struct sockaddr_ns *snstab[] = {
SNS(ridreq.ifr_addr), SNS(addreq.ifra_addr),
SNS(addreq.ifra_mask), SNS(addreq.ifra_broadaddr)};

xns_getaddr(addr, which)
char *addr;
{
	struct sockaddr_ns *sns = snstab[which];
	struct ns_addr ns_addr();

	sns->sns_family = AF_NS;
	sns->sns_len = sizeof(*sns);
	sns->sns_addr = ns_addr(addr);
	if (which == MASK)
		printf("Attempt to set XNS netmask will be ineffectual\n");
}

#define SISO(x) ((struct sockaddr_iso *) &(x))
struct sockaddr_iso *sisotab[] = {
SISO(iso_ridreq.ifr_Addr), SISO(iso_addreq.ifra_addr),
SISO(iso_addreq.ifra_mask), SISO(iso_addreq.ifra_dstaddr)};

iso_getaddr(addr, which)
char *addr;
{
	register struct sockaddr_iso *siso = sisotab[which];
	struct iso_addr *iso_addr();
	siso->siso_addr = *iso_addr(addr);

	if (which == MASK) {
		siso->siso_len = TSEL(siso) - (caddr_t)(siso);
		siso->siso_nlen = 0;
	} else {
		siso->siso_len = sizeof(*siso);
		siso->siso_family = AF_ISO;
	}
}

setnsellength(val)
	char *val;
{
	nsellength = atoi(val);
	if (nsellength < 0)
		errx(1, "Negative NSEL length is absurd");
	if (afp == 0 || afp->af_af != AF_ISO)
		errx(1, "Setting NSEL length valid only for iso");
}

fixnsel(s)
register struct sockaddr_iso *s;
{
	if (s->siso_family == 0)
		return;
	s->siso_tlen = nsellength;
}

adjust_nsellength()
{
	fixnsel(sisotab[RIDADDR]);
	fixnsel(sisotab[ADDR]);
	fixnsel(sisotab[DSTADDR]);
}
