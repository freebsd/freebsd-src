/*
 * print serial line IP statistics:
 *	slstat [-i interval] [-v] [interface] [system] [core]
 *
 * Copyright (c) 1989, 1990, 1991, 1992 Regents of the University of
 * California. All rights reserved.
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
 *	Van Jacobson (van@ee.lbl.gov), Dec 31, 1989:
 *	- Initial distribution.
 */

#ifndef lint
static char rcsid[] = "$Id: slstat.c,v 1.4 1995/05/30 03:52:30 rgrimes Exp $";
#endif

#include <stdio.h>
#include <paths.h>
#include <nlist.h>
#include <kvm.h>

#define INET

#include <limits.h>
#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/file.h>
#include <errno.h>
#include <signal.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <net/slcompress.h>
#include <net/if_slvar.h>

struct nlist nl[] = {
#define N_SOFTC 0
	{ "_sl_softc" },
	{ 0 }
};

#define INTERFACE_PREFIX        "sl%d"
char    interface[IFNAMSIZ];

const char	*system = NULL;
char	*kmemf = NULL;

kvm_t	*kvm_h;
int	kflag;
int	rflag;
int	vflag;
unsigned interval = 5;
int	unit;

extern	char *malloc();
extern	off_t lseek();

main(argc, argv)
	int argc;
	char *argv[];
{
	int c;
	char errbuf[_POSIX2_LINE_MAX];

	system = getbootfile();

	--argc; ++argv;
	while (argc > 0) {
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
	kvm_h = kvm_openfiles(system, kmemf, NULL, O_RDONLY, errbuf);
	if (kvm_h == 0) {
		(void)fprintf(stderr,
		    "slstat: kvm_openfiles(%s,%s,0): %s\n",
			      system, kmemf, errbuf);
		exit(1);
	}
	if ((c = kvm_nlist(kvm_h, nl)) != 0) {
		if (c > 0) {
			(void)fprintf(stderr,
			    "slstat: undefined symbols in %s:", system);
			for (c = 0; c < sizeof(nl)/sizeof(nl[0]); c++)
				if (nl[c].n_type == 0)
					fprintf(stderr, " %s", nl[c].n_name);
			(void)fputc('\n', stderr);
		} else
			(void)fprintf(stderr, "slstat: kvm_nlist: %s\n",
			    kvm_geterr(kvm_h));
		exit(1);
	}
	intpr();
	exit(0);
}

#define V(offset) ((line % 20)? ((sc->offset - osc->offset) / \
		  (rflag ? interval : 1)) : sc->offset)
#define AMT (sizeof(*sc) - 2 * sizeof(sc->sc_comp.tstate))

usage()
{
	static char umsg[] =
		"usage: slstat [-i interval] [-v] [unit] [system] [core]\n";

	fprintf(stderr, umsg);
	exit(1);
}

u_char	signalled;			/* set if alarm goes off "early" */

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
	void catchalarm();
	struct sl_softc *sc, *osc;
	off_t addr;

	addr = nl[N_SOFTC].n_value + unit * sizeof(struct sl_softc);
	sc = (struct sl_softc *)malloc(AMT);
	osc = (struct sl_softc *)malloc(AMT);
	bzero((char *)osc, AMT);

	while (1) {
		if (kvm_read(kvm_h, addr, (char *)sc, AMT) < 0)
			perror("kmem read");
		(void)signal(SIGALRM, catchalarm);
		signalled = 0;
		(void)alarm(interval);

		if ((line % 20) == 0) {
			printf("%8.8s %6.6s %6.6s %6.6s %6.6s",
				"in", "pack", "comp", "uncomp", "unknwn");
			if (vflag)
				printf(" %6.6s %6.6s %6.6s",
				       "toss", "other", "err");
			printf(" | %8.8s %6.6s %6.6s %6.6s %6.6s",
				"out", "pack", "comp", "uncomp", "other");
			if (vflag)
				printf(" %6.6s %6.6s %6.6s %6.6s",
				       "search", "miss", "err", "coll");
			putchar('\n');
		}
		printf("%8u %6d %6u %6u %6u",
		        V(sc_if.if_ibytes),
			V(sc_if.if_ipackets),
			V(sc_comp.sls_compressedin),
			V(sc_comp.sls_uncompressedin),
			V(sc_comp.sls_errorin));
		if (vflag)
			printf(" %6u %6u %6u",
				V(sc_comp.sls_tossed),
				V(sc_if.if_ipackets) -
				  V(sc_comp.sls_compressedin) -
				  V(sc_comp.sls_uncompressedin) -
				  V(sc_comp.sls_errorin),
			       V(sc_if.if_ierrors));
		printf(" | %8u %6d %6u %6u %6u",
			V(sc_if.if_obytes) / (rflag ? interval : 1),
			V(sc_if.if_opackets),
			V(sc_comp.sls_compressed),
			V(sc_comp.sls_packets) - V(sc_comp.sls_compressed),
			V(sc_if.if_opackets) - V(sc_comp.sls_packets));
		if (vflag)
			printf(" %6u %6u %6u %6u",
				V(sc_comp.sls_searches),
				V(sc_comp.sls_misses),
				V(sc_if.if_oerrors),
				V(sc_if.if_collisions));
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
		bcopy((char *)sc, (char *)osc, AMT);
	}
}

/*
 * Called if an interval expires before sidewaysintpr has completed a loop.
 * Sets a flag to not wait for the alarm.
 */
void
catchalarm()
{
	signalled = 1;
}

#if 0

#include <kvm.h>
#include <fcntl.h>

int kd;

kopen(system, kmemf, errstr)
	char *system;
	char *kmemf;
	char *errstr;
{
	if (strcmp(system, _PATH_UNIX) == 0 &&
	    strcmp(kmemf, _PATH_KMEM) == 0) {
		system = 0;
		kmemf = 0;
	}
	kd = kvm_openfiles(system, kmemf, (void *)0);
	if (kd == 0)
		return -1;

	return 0;
}

int
knlist(system, nl, errstr)
	char *system;
	struct nlist *nl;
	char *errstr;
{
	if (kd == 0)
		/* kopen() must be called first */
		abort();

	if (kvm_nlist(nl) < 0 || nl[0].n_type == 0) {
		fprintf(stderr, "%s: %s: no namelist\n", errstr, system);
		return -1;
	}
	return 0;
}

int
kread(addr, buf, size)
	off_t addr;
	char *buf;
	int size;
{
	if (kvm_read((char *)addr, buf, size) != size)
		return -1;
	return 0;
}
#endif

