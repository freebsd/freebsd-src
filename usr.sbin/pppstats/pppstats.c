/*
 * print PPP statistics:
 * 	pppstats [-i interval] [-v] [interface] [system] [core]
 *
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
static char rcsid[] = "$Id: pppstats.c,v 1.3 1994/11/19 13:57:06 jkh Exp $";
#endif

#include <ctype.h>
#include <errno.h>
#include <nlist.h>
#include <stdio.h>
#include <limits.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/file.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#define	VJC	1
#include <net/slcompress.h>

#ifndef STREAMS
#include <net/if_ppp.h>
#endif

#ifdef STREAMS
#define PPP_STATS	1	/* should be defined iff it is in ppp_if.c */
#include <sys/stream.h>
#include <net/ppp_str.h>
#endif

#define INTERFACE_PREFIX        "ppp%d"
char    interface[IFNAMSIZ];

#ifdef BSD4_4
#define KVMLIB
#endif

#ifndef KVMLIB

#include <machine/pte.h>
#ifdef ultrix
#include <machine/cpu.h>
#endif

struct	pte *Sysmap;
int	kmem;
char	*kmemf = "/dev/kmem";
extern	off_t lseek();

#else	/* KVMLIB */

char	*kmemf;

#if defined(sun) || defined(BSD4_4)
#include <kvm.h>
kvm_t	*kd;
#define KDARG	kd,

#else	/* sun */
#define KDARG
#endif	/* sun */

#endif	/* KVMLIB */

#ifdef STREAMS
struct nlist nl[] = {
#define N_SOFTC 0
	{ "_pii" },
	"",
};
#else
struct nlist nl[] = {
#define N_SOFTC 0
	{ "_ppp_softc" },
	"",
};
#endif

#ifndef BSD4_4
char	*system = "/vmunix";
#else
#include <paths.h>
#if defined(__FreeBSD__)
	/* _PATH_UNIX is defined as "Do not use _PATH_UNIX" */
char	*system = NULL;
#else
char	*system = _PATH_UNIX;
#endif
#endif

int	kflag;
int	vflag;
unsigned interval = 5;
int	unit;

extern	char *malloc();

main(argc, argv)
	int argc;
	char *argv[];
{

	char errbuf[_POSIX2_LINE_MAX];
	--argc; ++argv;
	while (argc > 0) {
		if (strcmp(argv[0], "-v") == 0) {
			++vflag;
			++argv, --argc;
			continue;
		}
		if (strcmp(argv[0], "-i") == 0 && argv[1] &&
		    isdigit(argv[1][0])) {
			interval = atoi(argv[1]);
			if (interval <= 0)
				usage();
			++argv, --argc;
			++argv, --argc;
			continue;
		}
		if (isdigit(argv[0][0])) {
			int s;
			struct ifreq ifr;

			unit = atoi(argv[0]);
			if (unit < 0)
				usage();
			sprintf(interface, INTERFACE_PREFIX, unit);
			s = socket(AF_INET, SOCK_DGRAM, 0);
			if (s < 0)
				err(1, "creating socket");
			strcpy(ifr.ifr_name, interface);
			if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0)
				errx(1,
				"unable to confirm existence of interface '%s'",
                    		interface);
			++argv, --argc;
			continue;
		}
		if (kflag)
			usage();

		system = *argv;
		++argv, --argc;
		if (argc > 0) {
			kmemf = *argv++;
			--argc;
			kflag++;
		}
	}
#ifndef KVMLIB
	if (nlist(system, nl) < 0 || nl[0].n_type == 0) {
		fprintf(stderr, "%s: no namelist\n", system);
		exit(1);
	}
	kmem = open(kmemf, O_RDONLY);
	if (kmem < 0) {
		perror(kmemf);
		exit(1);
	}
#ifndef ultrix
	if (kflag) {
		off_t off;

		Sysmap = (struct pte *)
		   malloc((u_int)(nl[N_SYSSIZE].n_value * sizeof(struct pte)));
		if (!Sysmap) {
			fputs("pppstats: can't get memory for Sysmap.\n", stderr);
			exit(1);
		}
		off = nl[N_SYSMAP].n_value & ~KERNBASE;
		(void)lseek(kmem, off, L_SET);
		(void)read(kmem, (char *)Sysmap,
		    (int)(nl[N_SYSSIZE].n_value * sizeof(struct pte)));
	}
#endif
#else
#if defined(sun)
	/* SunOS */
	if ((kd = kvm_open(system, kmemf, (char *)0, O_RDONLY, NULL)) == NULL) {
	    perror("kvm_open");
	    exit(1);
	}
#elif defined(BSD4_4)
	/* BSD4.4+ */
	if ((kd = kvm_openfiles(system, kmemf, NULL, O_RDONLY, errbuf)) == NULL) {
	    fprintf(stderr, "kvm_openfiles: %s", errbuf);
	    exit(1);
	}
#else
	/* BSD4.3+ */
	if (kvm_openfiles(system, kmemf, (char *)0) == -1) {
	    fprintf(stderr, "kvm_openfiles: %s", kvm_geterr());
	    exit(1);
	}
#endif

	if (kvm_nlist(KDARG nl)) {
	    fprintf(stderr, "pppstats: can't find symbols in nlist\n");
	    exit(1);
	}
#endif
	intpr();
	exit(0);
}

#ifndef KVMLIB
/*
 * Seek into the kernel for a value.
 */
off_t
klseek(fd, base, off)
	int fd, off;
	off_t base;
{
	if (kflag) {
#ifdef ultrix
		base = K0_TO_PHYS(base);
#else
		/* get kernel pte */
		base &= ~KERNBASE;
                base = ctob(Sysmap[btop(base)].pg_pfnum) + (base & PGOFSET);
#endif
	}
	return (lseek(fd, base, off));
}
#endif

usage()
{
	fprintf(stderr,"usage: pppstats [-i interval] [-v] [unit] [system] [core]\n");
	exit(1);
}

u_char	signalled;			/* set if alarm goes off "early" */

#define V(offset) ((line % 20)? sc->offset - osc->offset : sc->offset)

#ifdef STREAMS
#define STRUCT	struct ppp_if_info
#define	COMP	pii_sc_comp
#define	STATS	pii_ifnet
#else
#define STRUCT	struct ppp_softc
#define	COMP	sc_comp
#define	STATS	sc_if
#endif

/*
 * Print a running summary of interface statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
intpr()
{
	register int line = 0;
	int oldmask;
#ifdef __STDC__
	void catchalarm(int);
#else
	void catchalarm();
#endif

	STRUCT *sc, *osc;

	nl[N_SOFTC].n_value += unit * sizeof(STRUCT);
	sc = (STRUCT *)malloc(sizeof(STRUCT));
	osc = (STRUCT *)malloc(sizeof(STRUCT));

	bzero((char *)osc, sizeof(STRUCT));

	while (1) {
#ifndef KVMLIB
	    if (klseek(kmem, (off_t)nl[N_SOFTC].n_value, 0) == -1) {
		perror("kmem seek");
		exit(1);
	    }
	    if (read(kmem, (char *)sc, sizeof(STRUCT)) <= 0) {
		perror("kmem read");
		exit(1);
	    }
#else
	    if (kvm_read(KDARG nl[N_SOFTC].n_value, sc,
			 sizeof(STRUCT)) != sizeof(STRUCT)) {
		perror("kvm_read");
		exit(1);
	    }
#endif

	    (void)signal(SIGALRM, catchalarm);
	    signalled = 0;
	    (void)alarm(interval);

	    if ((line % 20) == 0) {
		printf("%6.6s %6.6s %6.6s %6.6s %6.6s",
		       "in", "pack", "comp", "uncomp", "err");
		if (vflag)
		    printf(" %6.6s %6.6s", "toss", "ip");
		printf(" | %6.6s %6.6s %6.6s %6.6s %6.6s",
		       "out", "pack", "comp", "uncomp", "ip");
		if (vflag)
		    printf(" %6.6s %6.6s", "search", "miss");
		putchar('\n');
	    }

	    printf("%6d %6d %6d %6d %6d",
#ifdef BSD4_4
		   V(STATS.if_ibytes),
#else
#ifndef STREAMS
		   V(sc_bytesrcvd),
#else
#ifdef PPP_STATS
		   V(pii_stats.ppp_ibytes),
#else
		   0,
#endif
#endif
#endif
		   V(STATS.if_ipackets),
		   V(COMP.sls_compressedin),
		   V(COMP.sls_uncompressedin),
		   V(COMP.sls_errorin));
	    if (vflag)
		printf(" %6d %6d",
		       V(COMP.sls_tossed),
		       V(STATS.if_ipackets) - V(COMP.sls_compressedin) -
		        V(COMP.sls_uncompressedin) - V(COMP.sls_errorin));
	    printf(" | %6d %6d %6d %6d %6d",
#ifdef BSD4_4
		   V(STATS.if_obytes),
#else
#ifndef STREAMS
		   V(sc_bytessent),
#else
#ifdef PPP_STATS
		   V(pii_stats.ppp_obytes),
#else
		   0,
#endif
#endif
#endif
		   V(STATS.if_opackets),
		   V(COMP.sls_compressed),
		   V(COMP.sls_packets) - V(COMP.sls_compressed),
		   V(STATS.if_opackets) - V(COMP.sls_packets));
	    if (vflag)
		printf(" %6d %6d",
		       V(COMP.sls_searches),
		       V(COMP.sls_misses));

	    putchar('\n');
	    fflush(stdout);
	    line++;

	    oldmask = sigblock(sigmask(SIGALRM));
	    if (! signalled) {
		sigpause(0);
	    }
	    sigsetmask(oldmask);
	    signalled = 0;
	    (void)alarm(interval);
	    bcopy((char *)sc, (char *)osc, sizeof(STRUCT));
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
