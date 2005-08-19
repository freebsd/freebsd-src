/*
 * Copyright (c) 1983, 1988, 1993
 *	Regents of the University of California.  All rights reserved.
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
char const copyright[] =
"@(#) Copyright (c) 1983, 1988, 1993\n\
	Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#if 0
#ifndef lint
static char sccsid[] = "@(#)main.c	8.4 (Berkeley) 3/1/94";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/file.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <netgraph/ng_socket.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <kvm.h>
#include <limits.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "netstat.h"

static struct nlist nl[] = {
#define	N_IFNET		0
	{ "_ifnet" },
#define	N_RTSTAT	1
	{ "_rtstat" },
#define N_RTREE		2
	{ "_rt_tables"},
#define N_MRTSTAT	3
	{ "_mrtstat" },
#define N_MFCTABLE	4
	{ "_mfctable" },
#define N_VIFTABLE	5
	{ "_viftable" },
#define N_IPX		6
	{ "_ipxpcb"},
#define N_IPXSTAT	7
	{ "_ipxstat"},
#define N_SPXSTAT	8
	{ "_spx_istat"},
#define N_DDPSTAT	9
	{ "_ddpstat"},
#define N_DDPCB		10
	{ "_ddpcb"},
#define N_NGSOCKS	11
	{ "_ngsocklist"},
#define N_IP6STAT	12
	{ "_ip6stat" },
#define N_ICMP6STAT	13
	{ "_icmp6stat" },
#define N_IPSECSTAT	14
	{ "_ipsecstat" },
#define N_IPSEC6STAT	15
	{ "_ipsec6stat" },
#define N_PIM6STAT	16
	{ "_pim6stat" },
#define N_MRT6STAT	17
	{ "_mrt6stat" },
#define N_MF6CTABLE	18
	{ "_mf6ctable" },
#define N_MIF6TABLE	19
	{ "_mif6table" },
#define N_PFKEYSTAT	20
	{ "_pfkeystat" },
#define N_MBSTAT	21
	{ "_mbstat" },
#define N_MBTYPES	22
	{ "_mbtypes" },
#define N_NMBCLUSTERS	23
	{ "_nmbclusters" },
#define N_NMBUFS	24
	{ "_nmbufs" },
#define	N_MBHI		25
	{ "_mbuf_hiwm" },
#define	N_CLHI		26
	{ "_clust_hiwm" },
#define	N_NCPUS		27
	{ "_smp_cpus" },
#define	N_PAGESZ	28
	{ "_pagesize" },
#define	N_MBPSTAT	29
	{ "_mb_statpcpu" },
#define	N_RTTRASH	30
	{ "_rttrash" },
#define	N_MBLO		31
	{ "_mbuf_lowm" },
#define	N_CLLO		32
	{ "_clust_lowm" },
#define N_CARPSTAT	33
	{ "_carpstats" },
	{ "" },
};

struct protox {
	u_char	pr_index;		/* index into nlist of cb head */
	u_char	pr_sindex;		/* index into nlist of stat block */
	u_char	pr_wanted;		/* 1 if wanted, 0 otherwise */
	void	(*pr_cblocks)(u_long, const char *, int);
					/* control blocks printing routine */
	void	(*pr_stats)(u_long, const char *, int);
					/* statistics printing routine */
	void	(*pr_istats)(char *);	/* per/if statistics printing routine */
	const char	*pr_name;		/* well-known name */
	u_long	pr_usesysctl;		/* non-zero if we use sysctl, not kvm */
} protox[] = {
	{ -1,		-1,		1,	protopr,
	  tcp_stats,	NULL,		"tcp",	IPPROTO_TCP },
	{ -1,		-1,		1,	protopr,
	  udp_stats,	NULL,		"udp",	IPPROTO_UDP },
	{ -1,		-1,		1,	protopr,
	  NULL,		NULL,		"divert",IPPROTO_DIVERT },
	{ -1,		-1,		1,	protopr,
	  ip_stats,	NULL,		"ip",	IPPROTO_RAW },
	{ -1,		-1,		1,	protopr,
	  icmp_stats,	NULL,		"icmp",	IPPROTO_ICMP },
	{ -1,		-1,		1,	protopr,
	  igmp_stats,	NULL,		"igmp",	IPPROTO_IGMP },
#ifdef IPSEC
	{ -1,		N_IPSECSTAT,	1,	NULL,
	  ipsec_stats,	NULL,		"ipsec",	0},
#endif
	{ -1,		-1,		1,	NULL,
	  bdg_stats,	NULL,		"bdg",	1 /* bridging... */ },
	{ -1,		-1,		1,	protopr,
	  pim_stats,	NULL,		"pim",	IPPROTO_PIM },
	{ -1,		N_CARPSTAT,	1,	0,
	  carp_stats,	NULL,		"carp",		0},
	{ -1,		-1,		0,	NULL,
	  NULL,		NULL,		NULL,	0 }
};

#ifdef INET6
struct protox ip6protox[] = {
	{ -1,		-1,		1,	protopr,
	  tcp_stats,	NULL,		"tcp",	IPPROTO_TCP },
	{ -1,		-1,		1,	protopr,
	  udp_stats,	NULL,		"udp",	IPPROTO_UDP },
	{ -1,		N_IP6STAT,	1,	protopr,
	  ip6_stats,	ip6_ifstats,	"ip6",	IPPROTO_RAW },
	{ -1,		N_ICMP6STAT,	1,	protopr,
	  icmp6_stats,	icmp6_ifstats,	"icmp6",IPPROTO_ICMPV6 },
#ifdef IPSEC
	{ -1,		N_IPSEC6STAT,	1,	NULL,
	  ipsec_stats,	NULL,		"ipsec6",0 },
#endif
#ifdef notyet
	{ -1,		N_PIM6STAT,	1,	NULL,
	  pim6_stats,	NULL,		"pim6",	0 },
#endif
	{ -1,		-1,		1,	NULL,
	  rip6_stats,	NULL,		"rip6",	0 },
	{ -1,		-1,		1,	NULL,
	  bdg_stats,	NULL,		"bdg",	1 /* bridging... */ },
	{ -1,		-1,		0,	NULL,
	  NULL,		NULL,		NULL,	0 }
};
#endif /*INET6*/

#ifdef IPSEC
struct protox pfkeyprotox[] = {
	{ -1,		N_PFKEYSTAT,	1,	NULL,
	  pfkey_stats,	NULL,		"pfkey", 0 },
	{ -1,		-1,		0,	NULL,
	  NULL,		NULL,		NULL,	0 }
};
#endif

struct protox atalkprotox[] = {
	{ N_DDPCB,	N_DDPSTAT,	1,	atalkprotopr,
	  ddp_stats,	NULL,		"ddp",	0 },
	{ -1,		-1,		0,	NULL,
	  NULL,		NULL,		NULL,	0 }
};

struct protox netgraphprotox[] = {
	{ N_NGSOCKS,	-1,		1,	netgraphprotopr,
	  NULL,		NULL,		"ctrl",	0 },
	{ N_NGSOCKS,	-1,		1,	netgraphprotopr,
	  NULL,		NULL,		"data",	0 },
	{ -1,		-1,		0,	NULL,
	  NULL,		NULL,		NULL,	0 }
};

struct protox ipxprotox[] = {
	{ N_IPX,	N_IPXSTAT,	1,	ipxprotopr,
	  ipx_stats,	NULL,		"ipx",	0 },
	{ N_IPX,	N_SPXSTAT,	1,	ipxprotopr,
	  spx_stats,	NULL,		"spx",	0 },
	{ -1,		-1,		0,	NULL,
	  NULL,		NULL,		0,	0 }
};

struct protox *protoprotox[] = {
					 protox,
#ifdef INET6
					 ip6protox,
#endif
#ifdef IPSEC
					 pfkeyprotox,
#endif
					 ipxprotox, atalkprotox, NULL };

const char *pluralies(int);
static void printproto(struct protox *, const char *);
static void usage(void);
static struct protox *name2protox(char *);
static struct protox *knownname(char *);

static kvm_t *kvmd;
static char *nlistf = NULL, *memf = NULL;

int	Aflag;		/* show addresses of protocol control block */
int	aflag;		/* show all sockets (including servers) */
int	bflag;		/* show i/f total bytes in/out */
int	dflag;		/* show i/f dropped packets */
int	gflag;		/* show group (multicast) routing or stats */
int	hflag;		/* show counters in human readable format */
int	iflag;		/* show interfaces */
int	Lflag;		/* show size of listen queues */
int	mflag;		/* show memory stats */
int	numeric_addr;	/* show addresses numerically */
int	numeric_port;	/* show ports numerically */
static int pflag;	/* show given protocol */
int	rflag;		/* show routing tables (or routing stats) */
int	sflag;		/* show protocol statistics */
int	tflag;		/* show i/f watchdog timers */
int	Wflag;		/* wide display */
int	zflag;		/* zero stats */

int	interval;	/* repeat interval for i/f stats */

char	*interface;	/* desired i/f for stats, or NULL for all i/fs */
int	unit;		/* unit number for above */

int	af;		/* address family */

int
main(int argc, char *argv[])
{
	struct protox *tp = NULL;  /* for printing cblocks & stats */
	int ch;

	af = AF_UNSPEC;

	while ((ch = getopt(argc, argv, "Aabcdf:ghI:iLlM:mN:np:rSstuWw:z")) != -1)
		switch(ch) {
		case 'A':
			Aflag = 1;
			break;
		case 'a':
			aflag = 1;
			break;
		case 'b':
			bflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'f':
			if (strcmp(optarg, "ipx") == 0)
				af = AF_IPX;
			else if (strcmp(optarg, "inet") == 0)
				af = AF_INET;
#ifdef INET6
			else if (strcmp(optarg, "inet6") == 0)
				af = AF_INET6;
#endif /*INET6*/
#ifdef INET6
			else if (strcmp(optarg, "pfkey") == 0)
				af = PF_KEY;
#endif /*INET6*/
			else if (strcmp(optarg, "unix") == 0)
				af = AF_UNIX;
			else if (strcmp(optarg, "atalk") == 0)
				af = AF_APPLETALK;
			else if (strcmp(optarg, "ng") == 0
			    || strcmp(optarg, "netgraph") == 0)
				af = AF_NETGRAPH;
			else if (strcmp(optarg, "link") == 0)
				af = AF_LINK;
			else {
				errx(1, "%s: unknown address family", optarg);
			}
			break;
		case 'g':
			gflag = 1;
			break;
		case 'h':
			hflag = 1;
			break;
		case 'I': {
			char *cp;

			iflag = 1;
			for (cp = interface = optarg; isalpha(*cp); cp++)
				continue;
			unit = atoi(cp);
			break;
		}
		case 'i':
			iflag = 1;
			break;
		case 'L':
			Lflag = 1;
			break;
		case 'M':
			memf = optarg;
			break;
		case 'm':
			mflag = 1;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'n':
			numeric_addr = numeric_port = 1;
			break;
		case 'p':
			if ((tp = name2protox(optarg)) == NULL) {
				errx(1, 
				     "%s: unknown or uninstrumented protocol",
				     optarg);
			}
			pflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 's':
			++sflag;
			break;
		case 'S':
			numeric_addr = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'u':
			af = AF_UNIX;
			break;
		case 'W':
		case 'l':
			Wflag = 1;
			break;
		case 'w':
			interval = atoi(optarg);
			iflag = 1;
			break;
		case 'z':
			zflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

#define	BACKWARD_COMPATIBILITY
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		if (isdigit(**argv)) {
			interval = atoi(*argv);
			if (interval <= 0)
				usage();
			++argv;
			iflag = 1;
		}
		if (*argv) {
			nlistf = *argv;
			if (*++argv)
				memf = *argv;
		}
	}
#endif

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (nlistf != NULL || memf != NULL)
		setgid(getgid());

	if (mflag) {
		if (memf != NULL) {
			if (kread(0, 0, 0) == 0)
				mbpr(nl[N_MBSTAT].n_value,
				    nl[N_MBTYPES].n_value,
				    nl[N_NMBCLUSTERS].n_value,
				    nl[N_NMBUFS].n_value,
				    nl[N_MBHI].n_value,
				    nl[N_CLHI].n_value,
				    nl[N_MBLO].n_value,
				    nl[N_CLLO].n_value,
				    nl[N_NCPUS].n_value,
				    nl[N_PAGESZ].n_value,
				    nl[N_MBPSTAT].n_value);
		} else
			mbpr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		exit(0);
	}
#if 0
	/*
	 * Keep file descriptors open to avoid overhead
	 * of open/close on each call to get* routines.
	 */
	sethostent(1);
	setnetent(1);
#else
	/*
	 * This does not make sense any more with DNS being default over
	 * the files.  Doing a setXXXXent(1) causes a tcp connection to be
	 * used for the queries, which is slower.
	 */
#endif
	if (iflag && !sflag) {
		kread(0, 0, 0);
		intpr(interval, nl[N_IFNET].n_value, NULL);
		exit(0);
	}
	if (rflag) {
		kread(0, 0, 0);
		if (sflag)
			rt_stats(nl[N_RTSTAT].n_value, nl[N_RTTRASH].n_value);
		else
			routepr(nl[N_RTREE].n_value);
		exit(0);
	}
	if (gflag) {
		kread(0, 0, 0);
		if (sflag) {
			if (af == AF_INET || af == AF_UNSPEC)
				mrt_stats(nl[N_MRTSTAT].n_value);
#ifdef INET6
			if (af == AF_INET6 || af == AF_UNSPEC)
				mrt6_stats(nl[N_MRT6STAT].n_value);
#endif
		} else {
			if (af == AF_INET || af == AF_UNSPEC)
				mroutepr(nl[N_MFCTABLE].n_value,
					 nl[N_VIFTABLE].n_value);
#ifdef INET6
			if (af == AF_INET6 || af == AF_UNSPEC)
				mroute6pr(nl[N_MF6CTABLE].n_value,
					  nl[N_MIF6TABLE].n_value);
#endif
		}
		ifmalist_dump();
		exit(0);
	}

	kread(0, 0, 0);
	if (tp) {
		printproto(tp, tp->pr_name);
		exit(0);
	}
	if (af == AF_INET || af == AF_UNSPEC)
		for (tp = protox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name);
#ifdef INET6
	if (af == AF_INET6 || af == AF_UNSPEC)
		for (tp = ip6protox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name);
#endif /*INET6*/
#ifdef IPSEC
	if (af == PF_KEY || af == AF_UNSPEC)
		for (tp = pfkeyprotox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name);
#endif /*IPSEC*/
	if (af == AF_IPX || af == AF_UNSPEC) {
		kread(0, 0, 0);
		for (tp = ipxprotox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name);
	}
	if (af == AF_APPLETALK || af == AF_UNSPEC)
		for (tp = atalkprotox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name);
	if (af == AF_NETGRAPH || af == AF_UNSPEC)
		for (tp = netgraphprotox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name);
	if ((af == AF_UNIX || af == AF_UNSPEC) && !Lflag && !sflag)
		unixpr();
	exit(0);
}

/*
 * Print out protocol statistics or control blocks (per sflag).
 * If the interface was not specifically requested, and the symbol
 * is not in the namelist, ignore this one.
 */
static void
printproto(tp, name)
	struct protox *tp;
	const char *name;
{
	void (*pr)(u_long, const char *, int);
	u_long off;

	if (sflag) {
		if (iflag) {
			if (tp->pr_istats)
				intpr(interval, nl[N_IFNET].n_value,
				      tp->pr_istats);
			else if (pflag)
				printf("%s: no per-interface stats routine\n",
				    tp->pr_name);
			return;
		}
		else {
			pr = tp->pr_stats;
			if (!pr) {
				if (pflag)
					printf("%s: no stats routine\n",
					    tp->pr_name);
				return;
			}
			off = tp->pr_usesysctl ? tp->pr_usesysctl 
				: nl[tp->pr_sindex].n_value;
		}
	} else {
		pr = tp->pr_cblocks;
		if (!pr) {
			if (pflag)
				printf("%s: no PCB routine\n", tp->pr_name);
			return;
		}
		off = tp->pr_usesysctl ? tp->pr_usesysctl
			: nl[tp->pr_index].n_value;
	}
	if (pr != NULL && (off || af != AF_UNSPEC))
		(*pr)(off, name, af);
}

/*
 * Read kernel memory, return 0 on success.
 */
int
kread(u_long addr, char *buf, int size)
{
	if (kvmd == 0) {
		/*
		 * XXX.
		 */
		kvmd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, buf);
		setgid(getgid());
		if (kvmd != NULL) {
			if (kvm_nlist(kvmd, nl) < 0) {
				if(nlistf)
					errx(1, "%s: kvm_nlist: %s", nlistf,
					     kvm_geterr(kvmd));
				else
					errx(1, "kvm_nlist: %s", kvm_geterr(kvmd));
			}

			if (nl[0].n_type == 0) {
				if(nlistf)
					errx(1, "%s: no namelist", nlistf);
				else
					errx(1, "no namelist");
			}
		} else {
			warnx("kvm not available");
			return(-1);
		}
	}
	if (!buf)
		return (0);
	if (kvm_read(kvmd, addr, buf, size) != size) {
		warnx("%s", kvm_geterr(kvmd));
		return (-1);
	}
	return (0);
}

const char *
plural(int n)
{
	return (n != 1 ? "s" : "");
}

const char *
plurales(int n)
{
	return (n != 1 ? "es" : "");
}

const char *
pluralies(int n)
{
	return (n != 1 ? "ies" : "y");
}

/*
 * Find the protox for the given "well-known" name.
 */
static struct protox *
knownname(char *name)
{
	struct protox **tpp, *tp;

	for (tpp = protoprotox; *tpp; tpp++)
		for (tp = *tpp; tp->pr_name; tp++)
			if (strcmp(tp->pr_name, name) == 0)
				return (tp);
	return (NULL);
}

/*
 * Find the protox corresponding to name.
 */
static struct protox *
name2protox(char *name)
{
	struct protox *tp;
	char **alias;			/* alias from p->aliases */
	struct protoent *p;

	/*
	 * Try to find the name in the list of "well-known" names. If that
	 * fails, check if name is an alias for an Internet protocol.
	 */
	if ((tp = knownname(name)) != NULL)
		return (tp);

	setprotoent(1);			/* make protocol lookup cheaper */
	while ((p = getprotoent()) != NULL) {
		/* assert: name not same as p->name */
		for (alias = p->p_aliases; *alias; alias++)
			if (strcmp(name, *alias) == 0) {
				endprotoent();
				return (knownname(p->p_name));
			}
	}
	endprotoent();
	return (NULL);
}

static void
usage(void)
{
	(void)fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
"usage: netstat [-AaLnSW] [-f protocol_family | -p protocol]\n"
"               [-M core] [-N system]",
"       netstat -i | -I interface [-abdhnt] [-f address_family]\n"
"               [-M core] [-N system]",
"       netstat -w wait [-I interface] [-d] [-M core] [-N system]",
"       netstat -s [-s] [-z] [-f protocol_family | -p protocol] [-M core]",
"       netstat -i | -I interface -s [-f protocol_family | -p protocol]\n"
"               [-M core] [-N system]",
"       netstat -m [-c] [-M core] [-N system]",
"       netstat -r [-AenW] [-f address_family] [-M core] [-N system]",
"       netstat -rs [-s] [-M core] [-N system]",
"       netstat -g [-W] [-f address_family] [-M core] [-N system]",
"       netstat -gs [-s] [-f address_family] [-M core] [-N system]");
	exit(1);
}
