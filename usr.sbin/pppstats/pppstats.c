/*
 * print PPP statistics:
 * 	pppstats [-i interval] [-v] [-r] [-c] [interface]
 *
 *   -i <update interval in seconds>
 *   -v Verbose mode for default display
 *   -r Show compression ratio in default display
 *   -c Show Compression statistics instead of default display
 *   -a Do not show relative values. Show absolute values at all times.
 *
 *
 * History:
 *      perkins@cps.msu.edu: Added compression statistics and alternate 
 *                display. 11/94

 *	Brad Parker (brad@cayman.com) 6/92
 *
 * from the original "slstats" by Van Jaconson
 *
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	Van Jacobson (van@helios.ee.lbl.gov), Dec 31, 1989:
 *	- Initial distribution.
 */

#ifndef lint
static char rcsid[] = "$Id: pppstats.c,v 1.11 1995/07/11 06:41:45 paulus Exp $";
#endif

#include <ctype.h>
#include <errno.h>
#include <nlist.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <net/ppp_defs.h>

#ifdef __svr4__
#include <sys/stropts.h>
#include <net/pppio.h>		/* SVR4, Solaris 2, etc. */

#else
#include <sys/socket.h>
#include <net/if.h>

#ifndef STREAMS
#include <net/if_ppp.h>		/* BSD, Linux, NeXT, etc. */

#else				/* SunOS 4, AIX 4, OSF/1, etc. */
#define PPP_STATS	1	/* should be defined iff it is in ppp_if.c */
#include <sys/stream.h>
#include <net/ppp_str.h>
#endif
#endif

int	vflag, rflag, cflag, aflag;
unsigned interval = 5;
int	unit;
int	s;			/* socket file descriptor */
int	signalled;		/* set if alarm goes off "early" */

extern	char *malloc();
void catchalarm __P((int));

main(argc, argv)
    int argc;
    char *argv[];
{
    --argc; ++argv;
    while (argc > 0) {
	if (strcmp(argv[0], "-a") == 0) {
	    ++aflag;
	    ++argv, --argc;
	    continue;
	}
	if (strcmp(argv[0], "-v") == 0) {
	    ++vflag;
	    ++argv, --argc;
	    continue;
	}
	if (strcmp(argv[0], "-r") == 0) {
	  ++rflag;
	  ++argv, --argc;
	  continue;
	}
	if (strcmp(argv[0], "-c") == 0) {
	  ++cflag;
	  ++argv, --argc;
	  continue;
	}
	if (strcmp(argv[0], "-i") == 0 && argv[1] &&
	    isdigit(argv[1][0])) {
	    interval = atoi(argv[1]);
	    if (interval < 0)
		usage();
	    ++argv, --argc;
	    ++argv, --argc;
	    continue;
	}
	if (isdigit(argv[0][0])) {
	    unit = atoi(argv[0]);
	    if (unit < 0)
		usage();
	    ++argv, --argc;
	    continue;
	}
	usage();
    }

#ifdef __svr4__
    if ((s = open("/dev/ppp", O_RDONLY)) < 0) {
	perror("pppstats: Couldn't open /dev/ppp: ");
	exit(1);
    }
    if (strioctl(s, PPPIO_ATTACH, &unit, sizeof(int), 0) < 0) {
	fprintf(stderr, "pppstats: ppp%d is not available\n", unit);
	exit(1);
    }
#else
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	perror("couldn't create IP socket");
	exit(1);
    }
#endif
    intpr();
    exit(0);
}

usage()
{
    fprintf(stderr, "Usage: pppstats [-v] [-r] [-c] [-i interval] [unit]\n");
    exit(1);
}

#define V(offset) (line % 20? cur.offset - old.offset: cur.offset)
#define W(offset) (line % 20? ccs.offset - ocs.offset: ccs.offset)

#define CRATE(comp, inc, unc)	((unc) == 0? 0.0: \
				 1.0 - (double)((comp) + (inc)) / (unc))

/*
 * Print a running summary of interface statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
intpr()
{
    register int line = 0;
    sigset_t oldmask, mask;
    struct ppp_stats cur, old;
    struct ppp_comp_stats ccs, ocs;

    memset(&old, 0, sizeof(old));
    memset(&ocs, 0, sizeof(ocs));

    while (1) {
	get_ppp_stats(&cur);
	if (cflag || rflag)
	    get_ppp_cstats(&ccs);

	(void)signal(SIGALRM, catchalarm);
	signalled = 0;
	(void)alarm(interval);
    
	if ((line % 20) == 0) {
	    if (line > 0)
		putchar('\n');
	    if (cflag) {
	    
		printf("%6.6s %6.6s %6.6s %6.6s %6.6s %6.6s %6.6s",
		       "ubyte", "upack", "cbyte", "cpack", "ibyte", "ipack", "ratio");
		printf(" | %6.6s %6.6s %6.6s %6.6s %6.6s %6.6s %6.6s",
		       "ubyte", "upack", "cbyte", "cpack", "ibyte", "ipack", "ratio");
		putchar('\n');
	    } else {

		printf("%6.6s %6.6s %6.6s %6.6s %6.6s",
		       "in", "pack", "comp", "uncomp", "err");
		if (vflag)
		    printf(" %6.6s %6.6s", "toss", "ip");
		if (rflag)
		    printf("   %6.6s %6.6s", "ratio", "ubyte");
		printf("  | %6.6s %6.6s %6.6s %6.6s %6.6s",
		       "out", "pack", "comp", "uncomp", "ip");
		if (vflag)
		    printf(" %6.6s %6.6s", "search", "miss");
		if(rflag)
		    printf("   %6.6s %6.6s", "ratio", "ubyte");
		putchar('\n');
	    }
	    memset(&old, 0, sizeof(old));
	    memset(&ocs, 0, sizeof(ocs));
	}
	
	if (cflag) {
	    printf("%6d %6d %6d %6d %6d %6d %6.2f",
		   W(d.unc_bytes),
		   W(d.unc_packets),
		   W(d.comp_bytes),
		   W(d.comp_packets),
		   W(d.inc_bytes),
		   W(d.inc_packets),
		   W(d.ratio) == 0? 0.0: 1 - 1.0 / W(d.ratio) * 256.0);

	    printf(" | %6d %6d %6d %6d %6d %6d %6.2f",
		   W(c.unc_bytes),
		   W(c.unc_packets),
		   W(c.comp_bytes),
		   W(c.comp_packets),
		   W(c.inc_bytes),
		   W(c.inc_packets),
		   W(d.ratio) == 0? 0.0: 1 - 1.0 / W(d.ratio) * 256.0);
	
	    putchar('\n');
	} else {

	    printf("%6d %6d %6d %6d %6d",
		   V(p.ppp_ibytes),
		   V(p.ppp_ipackets), V(vj.vjs_compressedin),
		   V(vj.vjs_uncompressedin), V(vj.vjs_errorin));
	    if (vflag)
		printf(" %6d %6d", V(vj.vjs_tossed),
		       V(p.ppp_ipackets) - V(vj.vjs_compressedin) -
		       V(vj.vjs_uncompressedin) - V(vj.vjs_errorin));
	    if (rflag)
		printf("   %6.2f %6d",
		       CRATE(W(d.comp_bytes), W(d.unc_bytes), W(d.unc_bytes)),
		       W(d.unc_bytes));
	    printf("  | %6d %6d %6d %6d %6d", V(p.ppp_obytes),
		   V(p.ppp_opackets), V(vj.vjs_compressed),
		   V(vj.vjs_packets) - V(vj.vjs_compressed),
		   V(p.ppp_opackets) - V(vj.vjs_packets));
	    if (vflag)
		printf(" %6d %6d", V(vj.vjs_searches), V(vj.vjs_misses));

	    if (rflag)
		printf("   %6.2f %6d",
		       CRATE(W(d.comp_bytes), W(d.unc_bytes), W(d.unc_bytes)),
		       W(c.unc_bytes));
	    
	    putchar('\n');
	}

	fflush(stdout);
	line++;
	if (interval == 0)
	    exit(0);

	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigprocmask(SIG_BLOCK, &mask, &oldmask);
	if (! signalled) {
	    sigemptyset(&mask);
	    sigsuspend(&mask);
	}
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
	signalled = 0;
	(void)alarm(interval);

	if (aflag==0) {
	    old = cur;
	    ocs = ccs;
	}
    }
}

/*
 * Called if an interval expires before sidewaysintpr has completed a loop.
 * Sets a flag to not wait for the alarm.
 */
void catchalarm(arg)
    int arg;
{
    signalled = 1;
}

#ifndef __svr4__
get_ppp_stats(curp)
    struct ppp_stats *curp;
{
    struct ifpppstatsreq req;

    memset (&req, 0, sizeof (req));

#ifdef _linux_
    req.stats_ptr = (caddr_t) &req.stats;
#undef ifr_name
#define ifr_name ifr__name
#endif

    sprintf(req.ifr_name, "ppp%d", unit);
    if (ioctl(s, SIOCGPPPSTATS, &req) < 0) {
	if (errno == ENOTTY)
	    fprintf(stderr, "pppstats: kernel support missing\n");
	else
	    perror("ioctl(SIOCGPPPSTATS)");
	exit(1);
    }
    *curp = req.stats;
}

get_ppp_cstats(csp)
    struct ppp_comp_stats *csp;
{
    struct ifpppcstatsreq creq;

    memset (&creq, 0, sizeof (creq));

#ifdef _linux_
    creq.stats_ptr = (caddr_t) &creq.stats;
#undef  ifr_name
#define ifr_name ifr__name
#endif

    sprintf(creq.ifr_name, "ppp%d", unit);
    if (ioctl(s, SIOCGPPPCSTATS, &creq) < 0) {
	if (errno == ENOTTY) {
	    fprintf(stderr, "pppstats: no kernel compression support\n");
	    if (cflag)
		exit(1);
	    rflag = 0;
	} else {
	    perror("ioctl(SIOCGPPPCSTATS)");
	    exit(1);
	}
    }
    *csp = creq.stats;
}

#else	/* __svr4__ */
get_ppp_stats(curp)
    struct ppp_stats *curp;
{
    if (strioctl(s, PPPIO_GETSTAT, curp, 0, sizeof(*curp)) < 0) {
	if (errno == EINVAL)
	    fprintf(stderr, "pppstats: kernel support missing\n");
	else
	    perror("pppstats: Couldn't get statistics");
	exit(1);
    }
}

get_ppp_cstats(csp)
    struct ppp_comp_stats *csp;
{
    if (strioctl(s, PPPIO_GETCSTAT, csp, 0, sizeof(*csp)) < 0) {
	if (errno == ENOTTY) {
	    fprintf(stderr, "pppstats: no kernel compression support\n");
	    if (cflag)
		exit(1);
	    rflag = 0;
	} else {
	    perror("pppstats: Couldn't get compression statistics");
	    exit(1);
	}
    }
}

int
strioctl(fd, cmd, ptr, ilen, olen)
    int fd, cmd, ilen, olen;
    char *ptr;
{
    struct strioctl str;

    str.ic_cmd = cmd;
    str.ic_timout = 0;
    str.ic_len = ilen;
    str.ic_dp = ptr;
    if (ioctl(fd, I_STR, &str) == -1)
	return -1;
    if (str.ic_len != olen)
	fprintf(stderr, "strioctl: expected %d bytes, got %d for cmd %x\n",
	       olen, str.ic_len, cmd);
    return 0;
}
#endif /* __svr4__ */
