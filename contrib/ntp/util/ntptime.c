/*
 * NTP test program
 *
 * This program tests to see if the NTP user interface routines
 * ntp_gettime() and ntp_adjtime() have been implemented in the kernel.
 * If so, each of these routines is called to display current timekeeping
 * data.
 *
 * For more information, see the README.kern file in the doc directory
 * of the xntp3 distribution.
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>
#include <signal.h>
#include <setjmp.h>

#include "ntp_fp.h"
#include "ntp_unixtime.h"
#include "ntp_syscall.h"
#include "ntp_stdlib.h"

#ifdef NTP_SYSCALLS_STD
# ifndef SYS_DECOSF1
#  define BADCALL -1		/* this is supposed to be a bad syscall */
# endif /* SYS_DECOSF1 */
#endif

#ifdef HAVE_TV_NSEC_IN_NTPTIMEVAL
#define tv_frac_sec tv_nsec
#else
#define tv_frac_sec tv_usec
#endif


#define TIMEX_MOD_BITS \
"\20\1OFFSET\2FREQUENCY\3MAXERROR\4ESTERROR\5STATUS\6TIMECONST\
\13PLL\14FLL\15MICRO\16NANO\17CLKB\20CLKA"
 
#define TIMEX_STA_BITS \
"\20\1PLL\2PPSFREQ\3PPSTIME\4FLL\5INS\6DEL\7UNSYNC\10FREQHOLD\
\11PPSSIGNAL\12PPSJITTER\13PPSWANDER\14PPSERROR\15CLOCKERR\
\16NANO\17MODE\20CLK"

#define SCALE_FREQ 65536		/* frequency scale */


/*
 * Function prototypes
 */
char *sprintb		P((u_int, const char *));
const char *timex_state	P((int));
volatile int debug = 0;

#ifdef SIGSYS
void pll_trap		P((int));

static struct sigaction newsigsys;	/* new sigaction status */
static struct sigaction sigsys;		/* current sigaction status */
static sigjmp_buf env;		/* environment var. for pll_trap() */
#endif

static volatile int pll_control; /* (0) daemon, (1) kernel loop */
static volatile int status;	/* most recent status bits */
static volatile int flash;	/* most recent ntp_adjtime() bits */
char* progname;
static char optargs[] = "cde:f:hm:o:rs:t:";

int
main(
	int argc,
	char *argv[]
	)
{
	extern int ntp_optind;
	extern char *ntp_optarg;
#ifdef SUBST_ADJTIMEX
      struct timex ntv;
#else
	struct ntptimeval ntv;
#endif
	struct timeval tv;
	struct timex ntx, _ntx;
	int	times[20];
	double ftemp, gtemp, htemp;
	long time_frac;				/* ntv.time.tv_frac_sec (us/ns) */
	l_fp ts;
	unsigned ts_mask = TS_MASK;		/* defaults to 20 bits (us) */
	unsigned ts_roundbit = TS_ROUNDBIT;	/* defaults to 20 bits (us) */
	int fdigits = 6;			/* fractional digits for us */
	int c;
	int errflg	= 0;
	int cost	= 0;
	int rawtime	= 0;

	memset((char *)&ntx, 0, sizeof(ntx));
	progname = argv[0];
	while ((c = ntp_getopt(argc, argv, optargs)) != EOF) switch (c) {
	    case 'c':
		cost++;
		break;
	    case 'd':
		debug++;
		break;
	    case 'e':
		ntx.modes |= MOD_ESTERROR;
		ntx.esterror = atoi(ntp_optarg);
		break;
	    case 'f':
		ntx.modes |= MOD_FREQUENCY;
		ntx.freq = (long)(atof(ntp_optarg) * SCALE_FREQ);
		break;
	    case 'm':
		ntx.modes |= MOD_MAXERROR;
		ntx.maxerror = atoi(ntp_optarg);
		break;
	    case 'o':
		ntx.modes |= MOD_OFFSET;
		ntx.offset = atoi(ntp_optarg);
		break;
	    case 'r':
		rawtime++;
		break;
	    case 's':
		ntx.modes |= MOD_STATUS;
		ntx.status = atoi(ntp_optarg);
		if (ntx.status < 0 || ntx.status > 4) errflg++;
		break;
	    case 't':
		ntx.modes |= MOD_TIMECONST;
		ntx.constant = atoi(ntp_optarg);
		break;
	    default:
		errflg++;
	}
	if (errflg || (ntp_optind != argc)) {
		(void) fprintf(stderr,
			       "usage: %s [-%s]\n\n\
-c		display the time taken to call ntp_gettime (us)\n\
-e esterror	estimate of the error (us)\n\
-f frequency	Frequency error (-500 .. 500) (ppm)\n\
-h		display this help info\n\
-m maxerror	max possible error (us)\n\
-o offset	current offset (ms)\n\
-r		print the unix and NTP time raw\n\
-l leap		Set the leap bits\n\
-t timeconstant	log2 of PLL time constant (0 .. %d)\n",
			       progname, optargs, MAXTC);
		exit(2);
	}

#ifdef SIGSYS
	/*
	 * Test to make sure the sigaction() works in case of invalid
	 * syscall codes.
	 */
	newsigsys.sa_handler = pll_trap;
	newsigsys.sa_flags = 0;
	if (sigaction(SIGSYS, &newsigsys, &sigsys)) {
		perror("sigaction() fails to save SIGSYS trap");
		exit(1);
	}
#endif /* SIGSYS */

#ifdef	BADCALL
	/*
	 * Make sure the trapcatcher works.
	 */
	pll_control = 1;
#ifdef SIGSYS
	if (sigsetjmp(env, 1) == 0)
	{
#endif
		status = syscall(BADCALL, &ntv); /* dummy parameter */
		if ((status < 0) && (errno == ENOSYS))
			--pll_control;
#ifdef SIGSYS
	}
#endif
	if (pll_control)
	    printf("sigaction() failed to catch an invalid syscall\n");
#endif /* BADCALL */

	if (cost) {
#ifdef SIGSYS
		if (sigsetjmp(env, 1) == 0) {
#endif
			for (c = 0; c < sizeof times / sizeof times[0]; c++) {
				status = ntp_gettime(&ntv);
				if ((status < 0) && (errno == ENOSYS))
					--pll_control;
				if (pll_control < 0)
					break;
				times[c] = ntv.time.tv_frac_sec;
			}
#ifdef SIGSYS
		}
#endif
		if (pll_control >= 0) {
			printf("[ us %06d:", times[0]);
			for (c = 1; c < sizeof times / sizeof times[0]; c++)
			    printf(" %d", times[c] - times[c - 1]);
			printf(" ]\n");
		}
	}
#ifdef SIGSYS
	if (sigsetjmp(env, 1) == 0) {
#endif
		status = ntp_gettime(&ntv);
		if ((status < 0) && (errno == ENOSYS))
			--pll_control;
#ifdef SIGSYS
	}
#endif
	_ntx.modes = 0;				/* Ensure nothing is set */
#ifdef SIGSYS
	if (sigsetjmp(env, 1) == 0) {
#endif
		status = ntp_adjtime(&_ntx);
		if ((status < 0) && (errno == ENOSYS))
			--pll_control;
		flash = _ntx.status;
#ifdef SIGSYS
	}
#endif
	if (pll_control < 0) {
		printf("NTP user interface routines are not configured in this kernel.\n");
		goto lexit;
	}

	/*
	 * Fetch timekeeping data and display.
	 */
	status = ntp_gettime(&ntv);
	if (status < 0)
		perror("ntp_gettime() call fails");
	else {
		printf("ntp_gettime() returns code %d (%s)\n",
		    status, timex_state(status));
		time_frac = ntv.time.tv_frac_sec;
#ifdef STA_NANO
		if (flash & STA_NANO) {
			ntv.time.tv_frac_sec /= 1000;
			ts_mask = 0xfffffffc;	/* 1/2^30 */
			ts_roundbit = 0x00000002;
			fdigits = 9;
		}
#endif
		tv.tv_sec = ntv.time.tv_sec;
		tv.tv_usec = ntv.time.tv_frac_sec;
		TVTOTS(&tv, &ts);
		ts.l_ui += JAN_1970;
		ts.l_uf += ts_roundbit;
		ts.l_uf &= ts_mask;
		printf("  time %s, (.%0*d),\n",
		       prettydate(&ts), fdigits, (int) time_frac);
		printf("  maximum error %lu us, estimated error %lu us.\n",
		       (u_long)ntv.maxerror, (u_long)ntv.esterror);
		if (rawtime) printf("  ntptime=%x.%x unixtime=%x.%0*d %s",
		    (unsigned int) ts.l_ui, (unsigned int) ts.l_uf,
		    (int) ntv.time.tv_sec, fdigits, (int) time_frac,
		    ctime((const time_t *) &ntv.time.tv_sec));
	}
	status = ntp_adjtime(&ntx);
	if (status < 0)
		perror((errno == EPERM) ? 
		   "Must be root to set kernel values\nntp_adjtime() call fails" :
		   "ntp_adjtime() call fails");
	else {
		flash = ntx.status;
		printf("ntp_adjtime() returns code %d (%s)\n",
		     status, timex_state(status));
		printf("  modes %s,\n", sprintb(ntx.modes, TIMEX_MOD_BITS));
		ftemp = (double)ntx.offset;
#ifdef STA_NANO
		if (flash & STA_NANO)
			ftemp /= 1000.0;
#endif
		printf("  offset %.3f", ftemp);
		ftemp = (double)ntx.freq / SCALE_FREQ;
		printf(" us, frequency %.3f ppm, interval %d s,\n",
		     ftemp, 1 << ntx.shift);
		printf("  maximum error %lu us, estimated error %lu us,\n",
		     (u_long)ntx.maxerror, (u_long)ntx.esterror);
		printf("  status %s,\n", sprintb((u_int)ntx.status, TIMEX_STA_BITS));
		ftemp = (double)ntx.tolerance / SCALE_FREQ;
		gtemp = (double)ntx.precision;
#ifdef STA_NANO
		if (flash & STA_NANO)
			gtemp /= 1000.0;
#endif
		printf(
		    "  time constant %lu, precision %.3f us, tolerance %.0f ppm,\n",
		    (u_long)ntx.constant, gtemp, ftemp);
		if (ntx.shift == 0)
			exit (0);
		ftemp = (double)ntx.ppsfreq / SCALE_FREQ;
		gtemp = (double)ntx.stabil / SCALE_FREQ;
		htemp = (double)ntx.jitter;
#ifdef STA_NANO
		if (flash & STA_NANO)
			htemp /= 1000.0;
#endif
		printf(
		    "  pps frequency %.3f ppm, stability %.3f ppm, jitter %.3f us,\n",
		    ftemp, gtemp, htemp);
		printf("  intervals %lu, jitter exceeded %lu, stability exceeded %lu, errors %lu.\n",
		    (u_long)ntx.calcnt, (u_long)ntx.jitcnt,
		    (u_long)ntx.stbcnt, (u_long)ntx.errcnt);
		return (0);
	}

	/*
	 * Put things back together the way we found them.
	 */
    lexit:
#ifdef SIGSYS
	if (sigaction(SIGSYS, &sigsys, (struct sigaction *)NULL)) {
		perror("sigaction() fails to restore SIGSYS trap");
		exit(1);
	}
#endif
	exit(0);
}

#ifdef SIGSYS
/*
 * pll_trap - trap processor for undefined syscalls
 */
void
pll_trap(
	int arg
	)
{
	pll_control--;
	siglongjmp(env, 1);
}
#endif

/*
 * Print a value a la the %b format of the kernel's printf
 */
char *
sprintb(
	register u_int v,
	register const char *bits
	)
{
	register char *cp;
	register int i, any = 0;
	register char c;
	static char buf[132];

	if (bits && *bits == 8)
	    (void)sprintf(buf, "0%o", v);
	else
	    (void)sprintf(buf, "0x%x", v);
	cp = buf + strlen(buf);
	bits++;
	if (bits) {
		*cp++ = ' ';
		*cp++ = '(';
		while ((i = *bits++) != 0) {
			if (v & (1 << (i-1))) {
				if (any)
				    *cp++ = ',';
				any = 1;
				for (; (c = *bits) > 32; bits++)
				    *cp++ = c;
			} else
			    for (; *bits > 32; bits++)
				continue;
		}
		*cp++ = ')';
	}
	*cp = '\0';
	return (buf);
}

const char *timex_states[] = {
	"OK", "INS", "DEL", "OOP", "WAIT", "ERROR"
};

const char *
timex_state(
	register int s
	)
{
	static char buf[32];

	if (s >= 0 && s <= sizeof(timex_states) / sizeof(timex_states[0]))
	    return (timex_states[s]);
	sprintf(buf, "TIME-#%d", s);
	return (buf);
}
