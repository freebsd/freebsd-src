/* @(#)rstat_proc.c	2.2 88/08/01 4.0 RPCSRC */
#ifndef lint
static  char sccsid[] = "@(#)rpc.rstatd.c 1.1 86/09/25 Copyr 1984 Sun Micro";
#endif

/*
 * Copyright (c) 1984 by Sun Microsystems, Inc.
 */

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/*
 * rstat service:  built with rstat.x and derived from rpc.rstatd.c
 */

#include <signal.h>
#include <stdio.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <nlist.h>
#include <sys/dk.h>
#include <sys/errno.h>
#include <sys/vmmeter.h>
#include <net/if.h>
#include "rstat.h"

struct nlist nl[] = {
#define	X_CPTIME	0
	{ "_cp_time" },
#define	X_SUM		1
	{ "_sum" },
#define	X_IFNET		2
	{ "_ifnet" },
#define	X_DKXFER	3
	{ "_dk_xfer" },
#define	X_BOOTTIME	4
	{ "_boottime" },
#define	X_AVENRUN	5
	{ "_avenrun" },
#define X_HZ		6
	{ "_hz" },
	"",
};
int kmem;
int firstifnet, numintfs;	/* chain of ethernet interfaces */
int stats_service();

/*
 *  Define EXIT_WHEN_IDLE if you are able to have this program invoked
 *  automatically on demand (as from inetd).  When defined, the service
 *  will terminated after being idle for 20 seconds.
 */
int sincelastreq = 0;		/* number of alarms since last request */
#ifdef EXIT_WHEN_IDLE
#define CLOSEDOWN 20		/* how long to wait before exiting */
#endif /* def EXIT_WHEN_IDLE */

union {
    struct stats s1;
    struct statsswtch s2;
    struct statstime s3;
} stats_all;

int updatestat();
static stat_is_init = 0;
extern int errno;

#ifndef FSCALE
#define FSCALE (1 << 8)
#endif

stat_init()
{
    stat_is_init = 1;
	setup();
	updatestat();
	alarm(1);
	signal(SIGALRM, updatestat);
    sleep(1);               /* allow for one wake-up */
}

statstime *
rstatproc_stats_3()
{
    if (! stat_is_init)
        stat_init();
    sincelastreq = 0;
    return(&stats_all.s3);
}

statsswtch *
rstatproc_stats_2()
{
    if (! stat_is_init)
        stat_init();
    sincelastreq = 0;
    return(&stats_all.s2);
}

stats *
rstatproc_stats_1()
{
    if (! stat_is_init)
        stat_init();
    sincelastreq = 0;
    return(&stats_all.s1);
}

u_int *
rstatproc_havedisk_3()
{
    static u_int have;

    if (! stat_is_init)
        stat_init();
    sincelastreq = 0;
    have = havedisk();
	return(&have);
}

u_int *
rstatproc_havedisk_2()
{
    return(rstatproc_havedisk_3());
}

u_int *
rstatproc_havedisk_1()
{
    return(rstatproc_havedisk_3());
}

updatestat()
{
	int off, i, hz;
	struct vmmeter sum;
	struct ifnet ifnet;
	double avrun[3];
	struct timeval tm, btm;

#ifdef DEBUG
	fprintf(stderr, "entering updatestat\n");
#endif
#ifdef EXIT_WHEN_IDLE
	if (sincelastreq >= CLOSEDOWN) {
#ifdef DEBUG
	fprintf(stderr, "about to closedown\n");
#endif
		exit(0);
	}
	sincelastreq++;
#endif /* def EXIT_WHEN_IDLE */
	if (lseek(kmem, (long)nl[X_HZ].n_value, 0) == -1) {
		fprintf(stderr, "rstat: can't seek in kmem\n");
		exit(1);
	}
	if (read(kmem, (char *)&hz, sizeof hz) != sizeof hz) {
		fprintf(stderr, "rstat: can't read hz from kmem\n");
		exit(1);
	}
	if (lseek(kmem, (long)nl[X_CPTIME].n_value, 0) == -1) {
		fprintf(stderr, "rstat: can't seek in kmem\n");
		exit(1);
	}
 	if (read(kmem, (char *)stats_all.s1.cp_time, sizeof (stats_all.s1.cp_time))
	    != sizeof (stats_all.s1.cp_time)) {
		fprintf(stderr, "rstat: can't read cp_time from kmem\n");
		exit(1);
	}
	if (lseek(kmem, (long)nl[X_AVENRUN].n_value, 0) ==-1) {
		fprintf(stderr, "rstat: can't seek in kmem\n");
		exit(1);
	}
#ifdef vax
 	if (read(kmem, (char *)avrun, sizeof (avrun)) != sizeof (avrun)) {
		fprintf(stderr, "rstat: can't read avenrun from kmem\n");
		exit(1);
	}
	stats_all.s2.avenrun[0] = avrun[0] * FSCALE;
	stats_all.s2.avenrun[1] = avrun[1] * FSCALE;
	stats_all.s2.avenrun[2] = avrun[2] * FSCALE;
#endif
	if (lseek(kmem, (long)nl[X_BOOTTIME].n_value, 0) == -1) {
		fprintf(stderr, "rstat: can't seek in kmem\n");
		exit(1);
	}
 	if (read(kmem, (char *)&btm, sizeof (stats_all.s2.boottime))
	    != sizeof (stats_all.s2.boottime)) {
		fprintf(stderr, "rstat: can't read boottime from kmem\n");
		exit(1);
	}
	stats_all.s2.boottime.tv_sec = btm.tv_sec;
	stats_all.s2.boottime.tv_usec = btm.tv_usec;


#ifdef DEBUG
	fprintf(stderr, "%d %d %d %d\n", stats_all.s1.cp_time[0],
	    stats_all.s1.cp_time[1], stats_all.s1.cp_time[2], stats_all.s1.cp_time[3]);
#endif

	if (lseek(kmem, (long)nl[X_SUM].n_value, 0) ==-1) {
		fprintf(stderr, "rstat: can't seek in kmem\n");
		exit(1);
	}
 	if (read(kmem, (char *)&sum, sizeof sum) != sizeof sum) {
		fprintf(stderr, "rstat: can't read sum from kmem\n");
		exit(1);
	}
	stats_all.s1.v_pgpgin = sum.v_pgpgin;
	stats_all.s1.v_pgpgout = sum.v_pgpgout;
	stats_all.s1.v_pswpin = sum.v_pswpin;
	stats_all.s1.v_pswpout = sum.v_pswpout;
	stats_all.s1.v_intr = sum.v_intr;
	gettimeofday(&tm, (struct timezone *) 0);
	stats_all.s1.v_intr -= hz*(tm.tv_sec - btm.tv_sec) +
	    hz*(tm.tv_usec - btm.tv_usec)/1000000;
	stats_all.s2.v_swtch = sum.v_swtch;

	if (lseek(kmem, (long)nl[X_DKXFER].n_value, 0) == -1) {
		fprintf(stderr, "rstat: can't seek in kmem\n");
		exit(1);
	}
 	if (read(kmem, (char *)stats_all.s1.dk_xfer, sizeof (stats_all.s1.dk_xfer))
	    != sizeof (stats_all.s1.dk_xfer)) {
		fprintf(stderr, "rstat: can't read dk_xfer from kmem\n");
		exit(1);
	}

	stats_all.s1.if_ipackets = 0;
	stats_all.s1.if_opackets = 0;
	stats_all.s1.if_ierrors = 0;
	stats_all.s1.if_oerrors = 0;
	stats_all.s1.if_collisions = 0;
	for (off = firstifnet, i = 0; off && i < numintfs; i++) {
		if (lseek(kmem, (long)off, 0) == -1) {
			fprintf(stderr, "rstat: can't seek in kmem\n");
			exit(1);
		}
		if (read(kmem, (char *)&ifnet, sizeof ifnet) != sizeof ifnet) {
			fprintf(stderr, "rstat: can't read ifnet from kmem\n");
			exit(1);
		}
		stats_all.s1.if_ipackets += ifnet.if_ipackets;
		stats_all.s1.if_opackets += ifnet.if_opackets;
		stats_all.s1.if_ierrors += ifnet.if_ierrors;
		stats_all.s1.if_oerrors += ifnet.if_oerrors;
		stats_all.s1.if_collisions += ifnet.if_collisions;
		off = (int) ifnet.if_next;
	}
	gettimeofday((struct timeval *)&stats_all.s3.curtime,
		(struct timezone *) 0);
	alarm(1);
}

static 
setup()
{
	struct ifnet ifnet;
	int off;

	nlist("/vmunix", nl);
	if (nl[0].n_value == 0) {
		fprintf(stderr, "rstat: Variables missing from namelist\n");
		exit (1);
	}
	if ((kmem = open("/dev/kmem", 0)) < 0) {
		fprintf(stderr, "rstat: can't open kmem\n");
		exit(1);
	}

	off = nl[X_IFNET].n_value;
	if (lseek(kmem, (long)off, 0) == -1) {
		fprintf(stderr, "rstat: can't seek in kmem\n");
		exit(1);
	}
	if (read(kmem, (char *)&firstifnet, sizeof(int)) != sizeof (int)) {
		fprintf(stderr, "rstat: can't read firstifnet from kmem\n");
		exit(1);
	}
	numintfs = 0;
	for (off = firstifnet; off;) {
		if (lseek(kmem, (long)off, 0) == -1) {
			fprintf(stderr, "rstat: can't seek in kmem\n");
			exit(1);
		}
		if (read(kmem, (char *)&ifnet, sizeof ifnet) != sizeof ifnet) {
			fprintf(stderr, "rstat: can't read ifnet from kmem\n");
			exit(1);
		}
		numintfs++;
		off = (int) ifnet.if_next;
	}
}

/*
 * returns true if have a disk
 */
static
havedisk()
{
	int i, cnt;
	long  xfer[DK_NDRIVE];

	nlist("/vmunix", nl);
	if (nl[X_DKXFER].n_value == 0) {
		fprintf(stderr, "rstat: Variables missing from namelist\n");
		exit (1);
	}
	if ((kmem = open("/dev/kmem", 0)) < 0) {
		fprintf(stderr, "rstat: can't open kmem\n");
		exit(1);
	}
	if (lseek(kmem, (long)nl[X_DKXFER].n_value, 0) == -1) {
		fprintf(stderr, "rstat: can't seek in kmem\n");
		exit(1);
	}
	if (read(kmem, (char *)xfer, sizeof xfer)!= sizeof xfer) {
		fprintf(stderr, "rstat: can't read kmem\n");
		exit(1);
	}
	cnt = 0;
	for (i=0; i < DK_NDRIVE; i++)
		cnt += xfer[i];
	return (cnt != 0);
}
