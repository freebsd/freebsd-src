/*
 * print serial line IP statistics:
 *	slstat [-i interval] [-v] [interface]
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_mib.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <net/slcompress.h>
#include <net/if_slvar.h>

static	void usage(void);
static	void intpr(void);
static	void catchalarm(int);

#define INTERFACE_PREFIX        "sl%d"
char    interface[IFNAMSIZ];

int	rflag;
int	vflag;
unsigned interval = 5;
int	unit;
int	name[6];

int
main(int argc, char *argv[])
{
	int c, i;
	size_t len;
	int maxifno;
	int indx;
	struct ifmibdata ifmd;

	while ((c = getopt(argc, argv, "vri:")) != -1) {
		switch(c) {
		case 'v':
			++vflag;
			break;
		case 'r':
			++rflag;
			break;
		case 'i':
			interval = atoi(optarg);
			if (interval <= 0)
				usage();
			break;
		default:
			usage();
		}
	}
	if (optind >= argc)
		sprintf(interface, INTERFACE_PREFIX, unit);
	else if (isdigit(argv[optind][0])) {
		unit = atoi(argv[optind]);
		if (unit < 0)
			usage();
		sprintf(interface, INTERFACE_PREFIX, unit);
	} else if (strncmp(argv[optind], "sl", 2) == 0
		  && isdigit(argv[optind][2])
		  && sscanf(argv[optind], "sl%d", &unit) == 1) {
		strncpy(interface, argv[optind], IFNAMSIZ);
	} else
		usage();

	name[0] = CTL_NET;
	name[1] = PF_LINK;
	name[2] = NETLINK_GENERIC;
	name[3] = IFMIB_SYSTEM;
	name[4] = IFMIB_IFCOUNT;
	len = sizeof maxifno;
	if (sysctl(name, 5, &maxifno, &len, 0, 0) < 0)
		err(1, "sysctl net.link.generic.system.ifcount");

	name[3] = IFMIB_IFDATA;
	name[5] = IFDATA_GENERAL;
	len = sizeof ifmd;
	for (i = 1; ; i++) {
		name[4] = i;

		if (sysctl(name, 6, &ifmd, &len, 0, 0) < 0) {
			if (errno == ENOENT)
				continue;

			err(1, "sysctl");
		}
		if (strncmp(interface, ifmd.ifmd_name, IFNAMSIZ) == 0
		    && ifmd.ifmd_data.ifi_type == IFT_SLIP) {
			indx = i;
			break;
		}
		if (i >= maxifno)
			errx(1, "interface %s does not exist", interface);
	}

	name[4] = indx;
	name[5] = IFDATA_LINKSPECIFIC;
	intpr();
	exit(0);
}

#define V(offset) ((line % 20)? ((sc->offset - osc->offset) / \
		  (rflag ? interval : 1)) : sc->offset)
#define AMT (sizeof(*sc) - 2 * sizeof(sc->sc_comp.tstate))

static void
usage()
{
	fprintf(stderr, "usage: slstat [-i interval] [-vr] [unit]\n");
	exit(1);
}

u_char	signalled;			/* set if alarm goes off "early" */

/*
 * Print a running summary of interface statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
static void
intpr()
{
	register int line = 0;
	int oldmask;
	struct sl_softc *sc, *osc;
	size_t len;

	sc = (struct sl_softc *)malloc(AMT);
	osc = (struct sl_softc *)malloc(AMT);
	bzero((char *)osc, AMT);
	len = AMT;

	while (1) {
		if (sysctl(name, 6, sc, &len, 0, 0) < 0 &&
		    (errno != ENOMEM || len != AMT))
			err(1, "sysctl linkspecific");

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
		printf("%8lu %6ld %6u %6u %6u",
		        V(sc_if.if_ibytes),
			V(sc_if.if_ipackets),
			V(sc_comp.sls_compressedin),
			V(sc_comp.sls_uncompressedin),
			V(sc_comp.sls_errorin));
		if (vflag)
			printf(" %6u %6lu %6lu",
				V(sc_comp.sls_tossed),
				V(sc_if.if_ipackets) -
				  V(sc_comp.sls_compressedin) -
				  V(sc_comp.sls_uncompressedin) -
				  V(sc_comp.sls_errorin),
			       V(sc_if.if_ierrors));
		printf(" | %8lu %6ld %6u %6u %6lu",
			V(sc_if.if_obytes) / (rflag ? interval : 1),
			V(sc_if.if_opackets),
			V(sc_comp.sls_compressed),
			V(sc_comp.sls_packets) - V(sc_comp.sls_compressed),
			V(sc_if.if_opackets) - V(sc_comp.sls_packets));
		if (vflag)
			printf(" %6u %6u %6lu %6lu",
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
static void
catchalarm(sig)
	int sig __unused;
{
	signalled = 1;
}
