/* ntp_unixclock.c,v 3.1 1993/07/06 01:11:30 jbj Exp
 * ntp_unixclock.c - routines for reading and adjusting a 4BSD-style
 *		     system clock
 */

#include <nlist.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#if defined(SYS_HPUX) || defined(sgi) || defined(SYS_BSDI)
#include <sys/param.h>
#include <utmp.h>
#endif

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_stdlib.h"

#if defined(HAVE_LIBKVM)
#ifdef	SYS_BSDI
#include <sys/proc.h>
#endif	/* SYS_BSDI */
#include <kvm.h>
#include <limits.h>

#ifndef _POSIX2_LINE_MAX
#define _POSIX2_LINE_MAX 2048
#endif
#endif	/* HAVE_LIBKVM */


#ifdef RS6000
#undef hz
#endif /* RS6000 */

extern int debug;
/*
 * These routines (init_systime, get_systime, step_systime, adj_systime)
 * implement an interface between the (more or less) system independent
 * bits of NTP and the peculiarities of dealing with the Unix system
 * clock.  These routines will run with good precision fairly independently
 * of your kernel's value of tickadj.  I couldn't tell the difference
 * between tickadj==40 and tickadj==5 on a microvax, though I prefer
 * to set tickadj == 500/hz when in doubt.  At your option you
 * may compile this so that your system's clock is always slewed to the
 * correct time even for large corrections.  Of course, all of this takes
 * a lot of code which wouldn't be needed with a reasonable tickadj and
 * a willingness to let the clock be stepped occasionally.  Oh well.
 */

/*
 * Clock variables.  We round calls to adjtime() to adj_precision
 * microseconds, and limit the adjustment to tvu_maxslew microseconds
 * (tsf_maxslew fractional sec) in one adjustment interval.  As we are
 * thus limited in the speed and precision with which we can adjust the
 * clock, we compensate by keeping the known "error" in the system time
 * in sys_clock_offset.  This is added to timestamps returned by get_systime().
 * We also remember the clock precision we computed from the kernel in
 * case someone asks us.
 */
extern	LONG adj_precision;	/* adj precision in usec (tickadj) */
extern	LONG tvu_maxslew;	/* maximum adjust doable in 1<<CLOCK_ADJ sec (usec) */

extern	U_LONG tsf_maxslew;	/* same as above, as LONG format */

extern	l_fp sys_clock_offset;	/* correction for current system time */

/*
 * Import sys_clock (it is updated in get_systime)
 */
extern LONG sys_clock;

static	void	clock_parms	P((U_LONG *, U_LONG *));

/*
 * init_systime - initialize the system clock support code, return
 *		  clock precision.
 *
 * Note that this code obtains to kernel variables related to the local
 * clock, tickadj and tick.  The code knows how the Berkeley adjtime
 * call works, and assumes these two variables are obtainable and are
 * used in the same manner.  Tick is supposed to be the number of
 * microseconds which are added to the system clock at clock interrupt
 * time when the time isn't being slewed.  Tickadj is supposed to be
 * the number of microseconds which are added or subtracted from tick when
 * the time is being slewed.
 *
 * If either of these two variables is missing, or is there but is used
 * for a purpose different than that described, you are SOL and may have
 * to do some custom kludging.
 *
 * This really shouldn't be in here.
 */
void
init_systime()
{
	U_LONG tickadj;
	U_LONG tick;
	U_LONG hz;

	/*
	 * Obtain the values
	 */
	clock_parms(&tickadj, &tick);
#ifdef	DEBUG
	if (debug)
		printf("kernel vars: tickadj = %d, tick = %d\n", tickadj, tick);
#endif

	/*
	 * If tickadj or hz wasn't found, we're doomed.  If hz is
	 * unreasonably small, forget it.
	 */
	if (tickadj == 0 || tick == 0) {
		syslog(LOG_ERR, "tickadj or tick unknown, exiting");
		exit(3);
	}
	if (tick > 65535) {
		syslog(LOG_ERR, "tick value of %lu is unreasonably large",
		    tick);
		exit(3);
	}

	/*
	 * Estimate hz from tick
	 */
	hz = 1000000L / tick;

	/*
	 * Set adj_precision and the maximum slew based on this.  Note
	 * that maxslew is set slightly shorter than it needs to be as
	 * insurance that all slews requested will complete in 1<<CLOCK_ADJ
	 * seconds.
	 */
#ifdef ADJTIME_IS_ACCURATE
	adj_precision = 1;
#else
	adj_precision = tickadj;
#endif /* ADJTIME_IS_ACCURATE */
#if defined(SLEWALWAYS) && !defined(ADJTIME_IS_ACCURATE)
	/*
	 * give us more time if we are always slewing... just in case
	 */
	tvu_maxslew = tickadj * (hz-3) * (1<<CLOCK_ADJ);
#else
	tvu_maxslew = tickadj * (hz-1) * (1<<CLOCK_ADJ);
#endif /* SLEWALWAYS */
	if (tvu_maxslew > 999990) {
		/*
		 * Don't let the maximum slew exceed 1 second in 4.  This
		 * simplifies calculations a lot since we can then deal
		 * with less-than-one-second fractions.
		 */
		tvu_maxslew = (999990/adj_precision) * adj_precision;
	}
	TVUTOTSF(tvu_maxslew, tsf_maxslew);
	syslog(LOG_NOTICE, "tickadj = %d, tick = %d, tvu_maxslew = %d",
	    tickadj, tick, tvu_maxslew);
#ifdef DEBUG
	if (debug)
		printf(
	"adj_precision = %d, tvu_maxslew = %d, tsf_maxslew = 0.%08x\n",
		    adj_precision, tvu_maxslew, tsf_maxslew);
#endif

	/*
	 * Set the current offset to 0
	 */
	sys_clock_offset.l_ui = sys_clock_offset.l_uf = 0;
}

#ifdef HAVE_LIBKVM
/*
 * clock_parms - return the local clock tickadj and tick parameters
 *
 * This version uses the SunOS libkvm (or the bsd compatability version).
 */
static void
clock_parms(tickadj, tick)
	U_LONG *tickadj;
	U_LONG *tick;
{
	static struct nlist nl[] = {
#define N_TICKADJ 0
		{ "_tickadj" },
#define N_TICK 1
		{ "_tick" },
		{ "" },
	};
#if	__convex__ /* { */
	if (K_open((char *)0,O_RDONLY,"/vmunix")!=0) {
		syslog(LOG_ERR, "K_open failed");
		exit(3);
	}
	kusenlist(1);
	if (knlist(nl)!=0
	||  nl[N_TICKADJ].n_value==0
	||  nl[N_TICK].n_value==0) {
		syslog(LOG_ERR, "knlist failed");
		exit(3);
	}
	if (K_read(tickadj,sizeof(*tickadj),nl[N_TICKADJ].n_value) !=
	    sizeof(*tickadj)) {
		syslog(LOG_ERR, "K_read tickadj failed");
		exit(3);
	}
	if (K_read(tick,sizeof(*tick),nl[N_TICK].n_value) !=
	    sizeof(*tick)) {
		syslog(LOG_ERR, "K_read tick failed");
		exit(3);
	}
	(void)K_close();
#else	/* }__convex__{ */
	register kvm_t *kd;
	if ((kd = kvm_open(NULL, NULL, NULL, O_RDONLY, NULL)) == NULL) {
		syslog(LOG_ERR, "kvm_open failed");
		exit(3);
	}
	if (kvm_nlist(kd, nl) != 0) {
		syslog(LOG_ERR, "kvm_nlist failed");
		exit(3);
	}
	if (kvm_read(kd, nl[N_TICKADJ].n_value, (char *)tickadj, sizeof(*tickadj)) !=
	    sizeof(*tickadj)) {
		syslog(LOG_ERR, "kvm_read tickadj failed");
		exit(3);
	}
	if (kvm_read(kd, nl[N_TICK].n_value, (char *)tick, sizeof(*tick)) !=
	    sizeof(*tick)) {
		syslog(LOG_ERR, "kvm_read tick failed");
		exit(3);
	}
	if (kvm_close(kd) < 0) {
		syslog(LOG_ERR, "kvm_close failed");
		exit(3);
	}
#endif	/*}convex*/
#undef N_TICKADJ
#undef N_TICK
}
#endif /* HAVE_LIBKVM */


#ifdef HAVE_READKMEM
/*
 * clock_parms - return the local clock tickadj and tick parameters
 *
 * Note that this version grovels about in /dev/kmem to determine
 * these values.  This probably should be elsewhere.
 */

/* Define the following to be what the tick and tickadj variables are 
 * called in your kernel. 
 */

#if defined(SYS_AUX3) || defined(SYS_AUX2) || defined(SYS_SVR4) || defined(SYS_PTX)
#define K_TICKADJ_NAME	"tickadj"
#define K_TICK_NAME		"tick"
#endif

#ifdef SYS_HPUX
#define K_TICKADJ_NAME	"_tickadj"
#define K_TICK_NAME		"_old_tick"
#endif

/* The defaults if not defined previously */
#if !defined(K_TICKADJ_NAME)
#define K_TICKADJ_NAME	"_tickadj"
#endif
#if !defined(K_TICK_NAME)
#define K_TICK_NAME		"_tick"
#endif

static void
clock_parms(tickadj, tick)
	U_LONG *tickadj;
	U_LONG *tick;
{
	register int i;
	int kmem;
#if defined(HAVE_N_UN)
#define N_NAME n_un.n_name
	static struct nlist nl[] =
	{	{{K_TICKADJ_NAME}},
		{{K_TICK_NAME}},
		{{""}},
	};
#else
#define N_NAME n_name
	static struct nlist nl[] =
	{	{K_TICKADJ_NAME},
		{K_TICK_NAME},
		{""},
	};
#endif
	static char *kernelnames[] = {
		"/vmunix",
		"/unix",
		"/mach",
		"/hp-ux",
		"/386bsd",
		"/netbsd",
#ifdef	KERNELFILE
		KERNELFILE,
#endif
		NULL
	};
	struct stat stbuf;
	int vars[2];

#define	K_TICKADJ	0
#define	K_TICK		1

	/* 
	 * Check to see what to use for the object file for names and get
	 * the locations of the necessary kernel variables.
	 */
	for (i = 0; kernelnames[i] != NULL; i++) {
		if (stat(kernelnames[i], &stbuf) == -1)
			continue;
		if (nlist(kernelnames[i], nl) >= 0)
			break;
	}
	if (kernelnames[i] == NULL) {
		syslog(LOG_ERR,
		  "Clock init couldn't find kernel object file");
		exit(3);
	}

	/*
	 * Read clock parameters from kernel
	 */
	kmem = open("/dev/kmem", O_RDONLY);
	if (kmem < 0) {
		syslog(LOG_ERR, "Can't open /dev/kmem for reading: %m");
#ifdef	DEBUG
		if (debug)
			perror("/dev/kmem");
#endif
		exit(3);
	}

	for (i = 0; i < (sizeof(vars)/sizeof(vars[0])); i++) {
		off_t where;

		vars[i] = 0;
		if ((where = nl[i].n_value) == 0) {
			syslog(LOG_ERR, "Unknown kernal var %s",
			       nl[i].N_NAME);
			continue;
		}
		if (lseek(kmem, where, SEEK_SET) == -1) {
			syslog(LOG_ERR, "lseek for %s fails: %m",
			       nl[i].N_NAME);
			continue;
		}
		if (read(kmem, &vars[i], sizeof(int)) != sizeof(int)) {
			syslog(LOG_ERR, "read for %s fails: %m",
			       nl[i].N_NAME);
		}
	}
	close(kmem);

	*tickadj = (U_LONG)vars[K_TICKADJ];
	*tick = (U_LONG)vars[K_TICK];

#undef	K_TICKADJ
#undef	K_TICK
#undef	K_TICKADJ_NAME
#undef	K_TICK_NAME
#undef	N_NAME
}
#endif /* HAVE_READKMEM */

#if defined(SOLARIS)&&defined(ADJTIME_IS_ACCURATE)
/*
 * clock_parms for Solaris 2.2 and later, with high-res timer kernel code.
 * The clock code changed in Solaris 2.2, and tickadj went away.
 * The good news is that ADJTIME_IS_ACCURATE and tick is available through 
 * sysconf().
 */
static void
clock_parms(tickadj, tick)
        U_LONG *tickadj;
        U_LONG *tick;
{
        int hz;

        hz = (int) sysconf (_SC_CLK_TCK);
        *tick = 1000000L/hz;
        *tickadj = (*tick/16); /* There is no tickadj, and it is only set here 
				for tvu_maxslew calculation above. Really,
				clock_parms should return adj_precision
				and tvu_maxslew, instead of the very 
				BSD-centric tickadj */

#ifdef DEBUG
        if (debug) printf ("Solaris tick = %d\n", *tick);
#endif
}
#endif /* SOLARIS_HRTIME */


#if defined(sgi)
/*
 * clock_parms - return the local clock tickadj and tick parameters
 *
 * The values set here were determined experimentally on a 4D/220 and
 * an R4000-50 server under IRIX 4.0.5.
 */
static void
clock_parms(tickadj, tick)
	U_LONG *tickadj;
	U_LONG *tick;
{
	*tick = 10000;
	*tickadj = 150;
}
#endif /* sgi */


#ifdef NOKMEM

#ifndef HZ
#define HZ	60
#endif

/*
 * clock_parms - return the local clock tickadj and tick parameters
 *
 * Note that this version uses static values!
 */
static void
clock_parms(tickadj, tick)
	U_LONG *tickadj;
	U_LONG *tick;
{
#ifdef RS6000
	*tickadj = 1000;
#else  /*RS6000*/
#if  SYS_DOMAINOS
	*tickadj = 668;
#else  /*SYS_DOMAINOS*/
	*tickadj = 500 / HZ;
#endif /*SYS_DOMAINOS*/
#endif /*RS6000*/
	*tick = 1000000L / HZ;

#ifdef	DEBUG
	if (debug)
		printf("NOTE: Using preset values for tick and tickadj !!\n");
#endif
}
#endif /*NOKMEM*/

#if ((defined(SOLARIS)&&!defined(ADJTIME_IS_ACCURATE))|| (defined(RS6000)&&!defined(NOKMEM))||defined(SYS_SINIXM) )
#ifndef _SC_CLK_TCK
#include <unistd.h>
#endif
/*
 * clock_parms - return the local clock tickadj and tick parameters
 *
 * Note that this version grovels about in /dev/kmem to determine
 * these values.  This probably should be elsewhere.
 */
static void
clock_parms(tickadj, tick)
	U_LONG *tickadj;
	U_LONG *tick;
{
	register int i;
	int kmem;
#define N_NAME n_name
	static struct nlist nl[] =
	{	{"tickadj"},
		{""},
	};
	static char *kernelnames[] = {
		"/kernel/unix",
		"/unix",
		NULL
	};
	struct stat stbuf;
	int vars[1];

#define	K_TICKADJ	0
	/*
	 * Read clock parameters from kernel
	 */
	kmem = open("/dev/kmem", O_RDONLY);
	if (kmem < 0) {
		syslog(LOG_ERR, "Can't open /dev/kmem for reading: %m");
#ifdef	DEBUG
		if (debug)
			perror("/dev/kmem");
#endif
		exit(3);
	}

	for (i = 0; kernelnames[i] != NULL; i++) {
		if (stat(kernelnames[i], &stbuf) == -1)
			continue;
		if (nlist(kernelnames[i], nl) >= 0)
			break;
	}
	if (kernelnames[i] == NULL) {
		syslog(LOG_ERR,
		  "Clock init couldn't find kernel as either /vmunix or /unix");
		exit(3);
	}

	for (i = 0; i < (sizeof(vars)/sizeof(vars[0])); i++) {
		off_t where;

		vars[i] = 0;
		if ((where = nl[i].n_value) == 0) {
			syslog(LOG_ERR, "Unknown kernal var %s",
			       nl[i].N_NAME);
			continue;
		}
		if (lseek(kmem, where, SEEK_SET) == -1) {
			syslog(LOG_ERR, "lseek for %s fails: %m",
			       nl[i].N_NAME);
			continue;
		}
		if (read(kmem, &vars[i], sizeof(int)) != sizeof(int)) {
			syslog(LOG_ERR, "read for %s fails: %m",
			       nl[i].N_NAME);
		}
#if defined(RS6000)
                /*
                 * Aix requires one more round of indirection.
                 */
                if (lseek(kmem, vars[i], SEEK_SET) == -1) {
                        syslog(LOG_ERR, "lseek for %s fails: %m",
                               nl[i].N_NAME);
                        continue;
                }
                if (read(kmem, &vars[i], sizeof(int)) != sizeof(int)) {
                        syslog(LOG_ERR, "read for %s fails: %m",
                               nl[i].N_NAME);
                }
#endif
	}
	close(kmem);

	*tickadj = (U_LONG)vars[K_TICKADJ];
	*tick = (U_LONG)(1000000/sysconf(_SC_CLK_TCK));

#undef	K_TICKADJ
#undef	N_NAME
}
#endif /* SOLARIS */

#ifdef SYS_LINUX
/* XXX should look this up somewhere ! */
static void
clock_parms(tickadj, tick)
	U_LONG *tickadj;
	U_LONG *tick;
{
      *tickadj = (U_LONG)1;
      *tick    = (U_LONG)10000;
}
#endif /* SYS_LINUX */
