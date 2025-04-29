/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/param.h>
#include <sys/file.h>
#ifdef JAIL
#include <sys/jail.h>
#endif
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <netinet/in.h>

#ifdef NETGRAPH
#include <netgraph/ng_socket.h>
#endif

#include <ctype.h>
#include <errno.h>
#ifdef JAIL
#include <jail.h>
#endif
#include <kvm.h>
#include <limits.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include "netstat.h"
#include "nl_defs.h"
#include <libxo/xo.h>

static struct protox {
	int	pr_index;		/* index into nlist of cb head */
	int	pr_sindex;		/* index into nlist of stat block */
	u_char	pr_wanted;		/* 1 if wanted, 0 otherwise */
	void	(*pr_cblocks)(u_long, const char *, int, int);
					/* control blocks printing routine */
	void	(*pr_stats)(u_long, const char *, int, int);
					/* statistics printing routine */
	void	(*pr_istats)(char *);	/* per/if statistics printing routine */
	const char	*pr_name;		/* well-known name */
	int	pr_usesysctl;		/* non-zero if we use sysctl, not kvm */
	int	pr_protocol;
} protox[] = {
	{ -1	,	N_TCPSTAT,	1,	protopr,
	  tcp_stats,	NULL,		"tcp",	1,	IPPROTO_TCP },
	{ -1	,	N_UDPSTAT,	1,	protopr,
	  udp_stats,	NULL,		"udp",	1,	IPPROTO_UDP },
#ifdef SCTP
	{ -1,		N_SCTPSTAT,	1,	sctp_protopr,
	  sctp_stats,	NULL,		"sctp",	1,	IPPROTO_SCTP },
#endif
#ifdef SDP
	{ -1,		-1,		1,	protopr,
	 NULL,		NULL,		"sdp",	1,	IPPROTO_TCP },
#endif
	{ -1	,	-1,		1,	protopr,
	  divert_stats,	NULL,		"divert", 1,	0 },
	{ -1	,	N_IPSTAT,	1,	protopr,
	  ip_stats,	NULL,		"ip",	1,	IPPROTO_RAW },
	{ -1	,	N_ICMPSTAT,	1,	protopr,
	  icmp_stats,	NULL,		"icmp",	1,	IPPROTO_ICMP },
	{ -1	,	N_IGMPSTAT,	1,	protopr,
	  igmp_stats,	NULL,		"igmp",	1,	IPPROTO_IGMP },
#ifdef IPSEC
	{ -1,		N_IPSEC4STAT,	1,	NULL,	/* keep as compat */
	  ipsec_stats,	NULL,		"ipsec", 1,	0},
	{ -1,		N_AHSTAT,	1,	NULL,
	  ah_stats,	NULL,		"ah",	1,	0},
	{ -1,		N_ESPSTAT,	1,	NULL,
	  esp_stats,	NULL,		"esp",	1,	0},
	{ -1,		N_IPCOMPSTAT,	1,	NULL,
	  ipcomp_stats,	NULL,		"ipcomp", 1,	0},
#endif
	{ -1	,	N_PIMSTAT,	1,	protopr,
	  pim_stats,	NULL,		"pim",	1,	IPPROTO_PIM },
	{ -1,		N_CARPSTATS,	1,	NULL,
	  carp_stats,	NULL,		"carp",	1,	0 },
#ifdef PF
	{ -1,		N_PFSYNCSTATS,	1,	NULL,
	  pfsync_stats,	NULL,		"pfsync", 1,	0 },
	{ -1,		N_PFLOWSTATS,	1,	NULL,
	  pflow_stats,	NULL,		"pflow", 1,	0 },
#endif
	{ -1,		N_ARPSTAT,	1,	NULL,
	  arp_stats,	NULL,		"arp", 1,	0 },
	{ -1,		-1,		0,	NULL,
	  NULL,		NULL,		NULL,	0,	0 }
};

#ifdef INET6
static struct protox ip6protox[] = {
	{ -1	,	N_TCPSTAT,	1,	protopr,
	  tcp_stats,	NULL,		"tcp",	1,	IPPROTO_TCP },
	{ -1	,	N_UDPSTAT,	1,	protopr,
	  udp_stats,	NULL,		"udp",	1,	IPPROTO_UDP },
	{ -1	,	N_IP6STAT,	1,	protopr,
	  ip6_stats,	ip6_ifstats,	"ip6",	1,	IPPROTO_RAW },
	{ -1	,	N_ICMP6STAT,	1,	protopr,
	  icmp6_stats,	icmp6_ifstats,	"icmp6", 1,	IPPROTO_ICMPV6 },
#ifdef SDP
	{ -1,		-1,		1,	protopr,
	 NULL,		NULL,		"sdp",	1,	IPPROTO_TCP },
#endif
#ifdef IPSEC
	{ -1,		N_IPSEC6STAT,	1,	NULL,
	  ipsec_stats,	NULL,		"ipsec6", 1,	0 },
#endif
#ifdef notyet
	{ -1,		N_PIM6STAT,	1,	NULL,
	  pim6_stats,	NULL,		"pim6",	1,	0 },
#endif
	{ -1,		N_RIP6STAT,	1,	NULL,
	  rip6_stats,	NULL,		"rip6",	1,	0 },
	{ -1,		-1,		0,	NULL,
	  NULL,		NULL,		NULL,	0,	0 }
};
#endif /*INET6*/

#ifdef IPSEC
static struct protox pfkeyprotox[] = {
	{ -1,		N_PFKEYSTAT,	1,	NULL,
	  pfkey_stats,	NULL,		"pfkey", 0,	0 },
	{ -1,		-1,		0,	NULL,
	  NULL,		NULL,		NULL,	0,	0 }
};
#endif

#ifdef NETGRAPH
static struct protox netgraphprotox[] = {
	{ N_NGSOCKLIST,	-1,		1,	netgraphprotopr,
	  NULL,		NULL,		"ctrl",	0,	0 },
	{ N_NGSOCKLIST,	-1,		1,	netgraphprotopr,
	  NULL,		NULL,		"data",	0,	0 },
	{ -1,		-1,		0,	NULL,
	  NULL,		NULL,		NULL,	0,	0 }
};
#endif

static struct protox *protoprotox[] = {
					 protox,
#ifdef INET6
					 ip6protox,
#endif
#ifdef IPSEC
					 pfkeyprotox,
#endif
					 NULL };

static void printproto(struct protox *, const char *, bool *);
static void usage(void) __dead2;
static struct protox *name2protox(const char *);
static struct protox *knownname(const char *);

static int kresolve_list(struct nlist *_nl);

static kvm_t *kvmd;
static char *nlistf = NULL, *memf = NULL;

bool	Aflag;		/* show addresses of protocol control block */
bool	aflag;		/* show all sockets (including servers) */
static bool	Bflag;		/* show information about bpf consumers */
bool	bflag;		/* show i/f total bytes in/out */
bool	cflag;		/* show TCP congestion control stack */
bool	Cflag;		/* show congestion control algo and vars */
bool	dflag;		/* show i/f dropped packets */
bool	gflag;		/* show group (multicast) routing or stats */
bool	hflag;		/* show counters in human readable format */
bool	iflag;		/* show interfaces */
bool	Lflag;		/* show size of listen queues */
bool	mflag;		/* show memory stats */
int	noutputs = 0;	/* how much outputs before we exit */
u_int	numeric_addr = 0; /* show addresses numerically */
bool	numeric_port;	/* show ports numerically */
bool	Oflag;		/* show nhgrp objects*/
bool	oflag;		/* show nexthop objects*/
bool	Pflag;		/* show TCP log ID */
static bool pflag;	/* show given protocol */
static bool Qflag;	/* show netisr information */
bool	rflag;		/* show routing tables (or routing stats) */
bool	Rflag;		/* show flow / RSS statistics */
int	sflag;		/* show protocol statistics */
bool	Wflag;		/* wide display */
bool	Tflag;		/* TCP Information */
bool	xflag;		/* extra information, includes all socket buffer info */
bool	zflag;		/* zero stats */

int	interval;	/* repeat interval for i/f stats */

char	*interface;	/* desired i/f for stats, or NULL for all i/fs */
int	unit;		/* unit number for above */
#ifdef JAIL
char	*jail_name;	/* desired jail to operate in */
#endif

static int	af;		/* address family */
int	live;		/* true if we are examining a live system */

int
main(int argc, char *argv[])
{
	struct protox *tp = NULL;  /* for printing cblocks & stats */
	int ch;
	int fib = -1;
	char *endptr;
	bool first = true;
#ifdef JAIL
	int jid;
#endif

	af = AF_UNSPEC;

	argc = xo_parse_args(argc, argv);
	if (argc < 0)
		exit(EXIT_FAILURE);

	while ((ch = getopt(argc, argv, "46AaBbCcdF:f:ghI:ij:LlM:mN:nOoPp:Qq:RrSTsuWw:xz"))
	    != -1)
		switch(ch) {
		case '4':
#ifdef INET
			af = AF_INET;
#else
			xo_errx(EX_UNAVAILABLE, "IPv4 support is not compiled in");
#endif
			break;
		case '6':
#ifdef INET6
			af = AF_INET6;
#else
			xo_errx(EX_UNAVAILABLE, "IPv6 support is not compiled in");
#endif
			break;
		case 'A':
			Aflag = true;
			break;
		case 'a':
			aflag = true;
			break;
		case 'B':
			Bflag = true;
			break;
		case 'b':
			bflag = true;
			break;
		case 'c':
			cflag = true;
			break;
		case 'C':
			Cflag = true;
			break;
		case 'd':
			dflag = true;
			break;
		case 'F':
			fib = strtol(optarg, &endptr, 0);
			if (*endptr != '\0' ||
			    (fib == 0 && (errno == EINVAL || errno == ERANGE)))
				xo_errx(EX_DATAERR, "%s: invalid fib", optarg);
			break;
		case 'f':
			if (strcmp(optarg, "inet") == 0)
				af = AF_INET;
#ifdef INET6
			else if (strcmp(optarg, "inet6") == 0)
				af = AF_INET6;
#endif
#ifdef IPSEC
			else if (strcmp(optarg, "pfkey") == 0)
				af = PF_KEY;
#endif
			else if (strcmp(optarg, "unix") == 0 ||
				 strcmp(optarg, "local") == 0)
				af = AF_UNIX;
#ifdef NETGRAPH
			else if (strcmp(optarg, "ng") == 0
			    || strcmp(optarg, "netgraph") == 0)
				af = AF_NETGRAPH;
#endif
			else if (strcmp(optarg, "link") == 0)
				af = AF_LINK;
			else {
				xo_errx(EX_DATAERR, "%s: unknown address family",
				    optarg);
			}
			break;
		case 'g':
			gflag = true;
			break;
		case 'h':
			hflag = true;
			break;
		case 'I': {
			char *cp;

			iflag = true;
			for (cp = interface = optarg; isalpha(*cp); cp++)
				continue;
			unit = atoi(cp);
			break;
		}
		case 'i':
			iflag = true;
			break;
		case 'j':
#ifdef JAIL
			if (optarg == NULL)
				usage();
			jail_name = optarg;
#else
			xo_errx(EX_UNAVAILABLE, "Jail support is not compiled in");
#endif
			break;
		case 'L':
			Lflag = true;
			break;
		case 'M':
			memf = optarg;
			break;
		case 'm':
			mflag = true;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'n':
			numeric_addr++;
			numeric_port = true;
			break;
		case 'o':
			oflag = true;
			break;
		case 'O':
			Oflag = true;
			break;
		case 'P':
			Pflag = true;
			break;
		case 'p':
			if ((tp = name2protox(optarg)) == NULL) {
				xo_errx(EX_DATAERR, "%s: unknown or uninstrumented "
				    "protocol", optarg);
			}
			pflag = true;
			break;
		case 'Q':
			Qflag = true;
			break;
		case 'q':
			noutputs = atoi(optarg);
			if (noutputs != 0)
				noutputs++;
			break;
		case 'r':
			rflag = true;
			break;
		case 'R':
			Rflag = true;
			break;
		case 's':
			++sflag;
			break;
		case 'S':
			numeric_addr = 1;
			break;
		case 'u':
			af = AF_UNIX;
			break;
		case 'W':
		case 'l':
			Wflag = true;
			break;
		case 'w':
			interval = atoi(optarg);
			iflag = true;
			break;
		case 'T':
			Tflag = true;
			break;
		case 'x':
			xflag = true;
			break;
		case 'z':
			zflag = true;
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
			iflag = true;
		}
		if (*argv) {
			nlistf = *argv;
			if (*++argv)
				memf = *argv;
		}
	}
#endif

#ifdef JAIL
	if (jail_name != NULL) {
		jid = jail_getid(jail_name);
		if (jid == -1)
			xo_errx(EX_UNAVAILABLE, "Jail not found");
		if (jail_attach(jid) != 0)
			xo_errx(EX_UNAVAILABLE, "Cannot attach to jail");
	}
#endif

	live = (nlistf == NULL && memf == NULL);
	/* Load all necessary kvm symbols */
	if (!live)
		kresolve_list(nl);

	if (xflag && Tflag)
		xo_errx(EX_USAGE, "-x and -T are incompatible, pick one.");

	if (Bflag) {
		if (!live)
			usage();
		bpf_stats(interface);
		if (xo_finish() < 0)
			xo_err(EX_IOERR, "stdout");
		exit(EX_OK);
	}
	if (mflag) {
		if (!live) {
			if (kread(0, NULL, 0) == 0)
				mbpr(kvmd, nl[N_SFSTAT].n_value);
		} else
			mbpr(NULL, 0);
		if (xo_finish() < 0)
			xo_err(EX_IOERR, "stdout");
		exit(EX_OK);
	}
	if (Qflag) {
		if (!live) {
			if (kread(0, NULL, 0) == 0)
				netisr_stats();
		} else
			netisr_stats();
		if (xo_finish() < 0)
			xo_err(EX_IOERR, "stdout");
		exit(EX_OK);
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
		xo_open_container("statistics");
		xo_set_version(NETSTAT_XO_VERSION);
		intpr(NULL, af);
		xo_close_container("statistics");
		if (xo_finish() < 0)
			xo_err(EX_IOERR, "stdout");
		exit(EX_OK);
	}
	if (rflag) {
		xo_open_container("statistics");
		xo_set_version(NETSTAT_XO_VERSION);
		if (sflag)
			rt_stats();
		else
			routepr(fib, af);
		xo_close_container("statistics");
		if (xo_finish() < 0)
			xo_err(EX_IOERR, "stdout");
		exit(EX_OK);
	}
	if (oflag) {
		xo_open_container("statistics");
		xo_set_version(NETSTAT_XO_VERSION);
		nhops_print(fib, af);
		xo_close_container("statistics");
		if (xo_finish() < 0)
			xo_err(EX_IOERR, "stdout");
		exit(EX_OK);
	}
	if (Oflag) {
		xo_open_container("statistics");
		xo_set_version(NETSTAT_XO_VERSION);
		nhgrp_print(fib, af);
		xo_close_container("statistics");
		if (xo_finish() < 0)
			xo_err(EX_IOERR, "stdout");
		exit(EX_OK);
	}



	if (gflag) {
		xo_open_container("statistics");
		xo_set_version(NETSTAT_XO_VERSION);
		if (sflag) {
			if (af == AF_INET || af == AF_UNSPEC)
				mrt_stats();
#ifdef INET6
			if (af == AF_INET6 || af == AF_UNSPEC)
				mrt6_stats();
#endif
		} else {
			if (af == AF_INET || af == AF_UNSPEC)
				mroutepr();
#ifdef INET6
			if (af == AF_INET6 || af == AF_UNSPEC)
				mroute6pr();
#endif
		}
		xo_close_container("statistics");
		if (xo_finish() < 0)
			xo_err(EX_IOERR, "stdout");
		exit(EX_OK);
	}

	if (tp) {
		xo_open_container("statistics");
		xo_set_version(NETSTAT_XO_VERSION);
		printproto(tp, tp->pr_name, &first);
		if (!first)
			xo_close_list("socket");
		xo_close_container("statistics");
		if (xo_finish() < 0)
			xo_err(EX_IOERR, "stdout");
		exit(EX_OK);
	}

	xo_open_container("statistics");
	xo_set_version(NETSTAT_XO_VERSION);
	if (af == AF_INET || af == AF_UNSPEC)
		for (tp = protox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name, &first);
#ifdef INET6
	if (af == AF_INET6 || af == AF_UNSPEC)
		for (tp = ip6protox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name, &first);
#endif /*INET6*/
#ifdef IPSEC
	if (af == PF_KEY || af == AF_UNSPEC)
		for (tp = pfkeyprotox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name, &first);
#endif /*IPSEC*/
#ifdef NETGRAPH
	if (af == AF_NETGRAPH || af == AF_UNSPEC)
		for (tp = netgraphprotox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name, &first);
#endif /* NETGRAPH */
	if ((af == AF_UNIX || af == AF_UNSPEC) && !sflag)
		unixpr(nl[N_UNP_COUNT].n_value, nl[N_UNP_GENCNT].n_value,
		    nl[N_UNP_DHEAD].n_value, nl[N_UNP_SHEAD].n_value,
		    nl[N_UNP_SPHEAD].n_value, &first);

	if (!first)
		xo_close_list("socket");
	xo_close_container("statistics");
	if (xo_finish() < 0)
		xo_err(EX_IOERR, "stdout");
	exit(EX_OK);
}

static int
fetch_stats_internal(const char *sysctlname, u_long off, void *stats,
    size_t len, kreadfn_t kreadfn, int zero)
{
	int error;

	if (live) {
		memset(stats, 0, len);
		if (zero)
			error = sysctlbyname(sysctlname, NULL, NULL, stats,
			    len);
		else
			error = sysctlbyname(sysctlname, stats, &len, NULL, 0);
		if (error == -1 && errno != ENOENT)
			xo_warn("sysctl %s", sysctlname);
	} else {
		if (off == 0)
			return (1);
		error = kreadfn(off, stats, len);
	}
	return (error);
}

int
fetch_stats(const char *sysctlname, u_long off, void *stats,
    size_t len, kreadfn_t kreadfn)
{

	return (fetch_stats_internal(sysctlname, off, stats, len, kreadfn,
    zflag));
}

int
fetch_stats_ro(const char *sysctlname, u_long off, void *stats,
    size_t len, kreadfn_t kreadfn)
{

	return (fetch_stats_internal(sysctlname, off, stats, len, kreadfn, 0));
}

/*
 * Print out protocol statistics or control blocks (per sflag).
 * If the interface was not specifically requested, and the symbol
 * is not in the namelist, ignore this one.
 */
static void
printproto(struct protox *tp, const char *name, bool *first)
{
	void (*pr)(u_long, const char *, int, int);
	u_long off;
	bool doingdblocks = false;

	if (sflag) {
		if (iflag) {
			if (tp->pr_istats)
				intpr(tp->pr_istats, af);
			else if (pflag)
				xo_message("%s: no per-interface stats routine",
				    tp->pr_name);
			return;
		} else {
			pr = tp->pr_stats;
			if (!pr) {
				if (pflag)
					xo_message("%s: no stats routine",
					    tp->pr_name);
				return;
			}
			if (tp->pr_usesysctl && live)
				off = 0;
			else if (tp->pr_sindex < 0) {
				if (pflag)
					xo_message("%s: stats routine doesn't "
					    "work on cores", tp->pr_name);
				return;
			} else
				off = nl[tp->pr_sindex].n_value;
		}
	} else {
		doingdblocks = true;
		pr = tp->pr_cblocks;
		if (!pr) {
			if (pflag)
				xo_message("%s: no PCB routine", tp->pr_name);
			return;
		}
		if (tp->pr_usesysctl && live)
			off = 0;
		else if (tp->pr_index < 0) {
			if (pflag)
				xo_message("%s: PCB routine doesn't work on "
				    "cores", tp->pr_name);
			return;
		} else
			off = nl[tp->pr_index].n_value;
	}
	if (pr != NULL && (off || (live && tp->pr_usesysctl) ||
	    af != AF_UNSPEC)) {
		if (doingdblocks && *first) {
			xo_open_list("socket");
			*first = false;
		}

		(*pr)(off, name, af, tp->pr_protocol);
	}
}

static int
kvmd_init(void)
{
	char errbuf[_POSIX2_LINE_MAX];

	if (kvmd != NULL)
		return (0);

	kvmd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, errbuf);
	if (kvmd == NULL) {
		xo_warnx("kvm not available: %s", errbuf);
		return (-1);
	}

	return (0);
}

/*
 * Resolve symbol list, return 0 on success.
 */
static int
kresolve_list(struct nlist *_nl)
{

	if ((kvmd == NULL) && (kvmd_init() != 0))
		return (-1);

	if (_nl[0].n_type != 0)
		return (0);

	if (kvm_nlist(kvmd, _nl) < 0) {
		if (nlistf)
			xo_errx(EX_UNAVAILABLE, "%s: kvm_nlist: %s", nlistf,
			    kvm_geterr(kvmd));
		else
			xo_errx(EX_UNAVAILABLE, "kvm_nlist: %s", kvm_geterr(kvmd));
	}

	return (0);
}

/*
 * Wrapper of kvm_dpcpu_setcpu().
 */
void
kset_dpcpu(u_int cpuid)
{

	if ((kvmd == NULL) && (kvmd_init() != 0))
		xo_errx(EX_UNAVAILABLE, "%s: kvm is not available", __func__);

	if (kvm_dpcpu_setcpu(kvmd, cpuid) < 0)
		xo_errx(EX_UNAVAILABLE, "%s: kvm_dpcpu_setcpu(%u): %s", __func__,
		    cpuid, kvm_geterr(kvmd)); 
	return;
}

/*
 * Read kernel memory, return 0 on success.
 */
int
kread(u_long addr, void *buf, size_t size)
{

	if (kvmd_init() < 0)
		return (-1);

	if (!buf)
		return (0);
	if (kvm_read(kvmd, addr, buf, size) != (ssize_t)size) {
		xo_warnx("%s", kvm_geterr(kvmd));
		return (-1);
	}
	return (0);
}

/*
 * Read single counter(9).
 */
uint64_t
kread_counter(u_long addr)
{

	if (kvmd_init() < 0)
		return (-1);

	return (kvm_counter_u64_fetch(kvmd, addr));
}

/*
 * Read an array of N counters in kernel memory into array of N uint64_t's.
 */
int
kread_counters(u_long addr, void *buf, size_t size)
{
	uint64_t *c;
	u_long *counters;
	size_t i, n;

	if (kvmd_init() < 0)
		return (-1);

	if (size % sizeof(uint64_t) != 0) {
		xo_warnx("kread_counters: invalid counter set size");
		return (-1);
	}

	n = size / sizeof(uint64_t);
	if ((counters = malloc(n * sizeof(u_long))) == NULL)
		xo_err(EX_OSERR, "malloc");
	if (kread(addr, counters, n * sizeof(u_long)) < 0) {
		free(counters);
		return (-1);
	}

	c = buf;
	for (i = 0; i < n; i++)
		c[i] = kvm_counter_u64_fetch(kvmd, counters[i]);

	free(counters);
	return (0);
}

const char *
plural(uintmax_t n)
{
	return (n != 1 ? "s" : "");
}

const char *
plurales(uintmax_t n)
{
	return (n != 1 ? "es" : "");
}

const char *
pluralies(uintmax_t n)
{
	return (n != 1 ? "ies" : "y");
}

/*
 * Find the protox for the given "well-known" name.
 */
static struct protox *
knownname(const char *name)
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
name2protox(const char *name)
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
	xo_error("%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
"usage: netstat [-j jail] [-46AaCcLnRSTWx] [-f protocol_family | -p protocol]\n"
"               [-M core] [-N system]",
"       netstat [-j jail] -i | -I interface [-46abdhnW] [-f address_family]\n"
"               [-M core] [-N system]",
"       netstat [-j jail] -w wait [-I interface] [-46d] [-M core] [-N system]\n"
"               [-q howmany]",
"       netstat [-j jail] -s [-46sz] [-f protocol_family | -p protocol]\n"
"               [-M core] [-N system]",
"       netstat [-j jail] -i | -I interface -s [-46s]\n"
"               [-f protocol_family | -p protocol] [-M core] [-N system]",
"       netstat [-j jail] -m [-M core] [-N system]",
"       netstat [-j jail] -B [-z] [-I interface]",
"       netstat [-j jail] -r [-46AnW] [-F fibnum] [-f address_family]\n"
"               [-M core] [-N system]",
"       netstat [-j jail] -rs [-s] [-M core] [-N system]",
"       netstat [-j jail] -g [-46W] [-f address_family] [-M core] [-N system]",
"       netstat [-j jail] -gs [-46s] [-f address_family] [-M core] [-N system]",
"       netstat [-j jail] -Q");
	exit(EX_USAGE);
}
