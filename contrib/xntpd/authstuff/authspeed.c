/* authspeed.c,v 3.1 1993/07/06 01:04:54 jbj Exp
 * authspeed - figure out how LONG it takes to do an NTP encryption
 */

#if defined(SYS_HPUX) || defined(SYS_AUX3) || defined(SYS_AUX2) || defined(SOLARIS) || defined(SYS_SVR4) || defined(SYS_PTX)
#define FAKE_RUSAGE
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#ifdef FAKE_RUSAGE
#include <sys/param.h>
#include <sys/times.h>
#endif

#include "ntp_fp.h"
#include "ntp_stdlib.h"

#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

#define DEFLOOPS	-1

#define	DEFDELAYLOOPS	20000
#define	DEFCOSTLOOPS	2000

char *progname;
int debug;

struct timeval tstart, tend;
#ifdef FAKE_RUSAGE
struct tms rstart, rend;
#define	getrusage(foo, t)	times(t)
#define	RUSAGE_SELF	0
#else
struct rusage rstart, rend;
#endif

l_fp dummy1, dummy2;
U_LONG dummy3;

U_LONG pkt[15];

int totalcost = 0;
double rtime;
double vtime;

int domd5 = 0;

static	void	dodelay	P((int));
static	void	docheap	P((int));
static	void	docost	P((int));
static	void	subtime	P((struct timeval *, struct timeval *, double *));

/*
 * main - parse arguments and handle options
 */
void
main(argc, argv)
int argc;
char *argv[];
{
	int c;
	int loops;
	int i;
	int errflg = 0;
	extern int ntp_optind;
	extern char *ntp_optarg;

	progname = argv[0];
	loops = DEFLOOPS;
	while ((c = ntp_getopt(argc, argv, "cdmn:")) != EOF)
		switch (c) {
		case 'c':
			totalcost++;
			break;
		case 'd':
			++debug;
			break;
		case 'm':
			domd5 = 16;	/* offset into list of keys */
			break;
		case 'n':
			loops = atoi(ntp_optarg);
			if (loops <= 0) {
				(void) fprintf(stderr, 
			"%s: %s is unlikely to be a useful number of loops\n",
					       progname, ntp_optarg);
				errflg++;
			}
			break;
		default:
			errflg++;
			break;
		}
	if (errflg || ntp_optind == argc) {
		(void) fprintf(stderr,
		    "usage: %s [-d] [-n loops] [ -c ] auth.samplekeys\n",
		    progname);
		exit(2);
	}
	printf("Compute timing for ");
	if (domd5)
	    printf("MD5");
	else
	    printf("DES");
	printf(" based authentication.\n");

	init_auth();
	authreadkeys(argv[ntp_optind]);
	for (i = 0; i < 16; i++) {
		if (!auth_havekey(i + domd5)) {
			errflg++;
			(void) fprintf(stderr, "%s: key %d missing\n",
			    progname, i + domd5);
		}
	}

	if (errflg) {
		(void) fprintf(stderr,
	"%s: check syslog for errors, or use file with complete set of keys\n",
		    progname);
		exit(1);
	}

	if (loops == DEFLOOPS) {
		if (totalcost)
			loops = DEFCOSTLOOPS;
		else
			loops = DEFDELAYLOOPS;
	}

	dummy1.l_ui = 0x80808080;
	dummy1.l_uf = 0xffffff00;
	dummy3 = 0x0aaaaaaa;

	for (i = 0; i < 12; i++)
		pkt[i] = i * 0x22222;

	if (totalcost) {
		if (totalcost > 1)
			docheap(loops);
		else
			docost(loops);
	} else {
		dodelay(loops);
	}

	printf("total real time: %.3f\n", rtime);
	printf("total CPU time: %.3f\n", vtime);
	if (totalcost) {
		printf("real cost (in seconds): %.6f\n",
		    rtime/(double)loops);
		printf("CPU cost (in seconds): %.6f\n",
		    vtime/(double)loops);
		printf("\nThis includes the cost of a decryption plus the\n");
		printf("the cost of an encryption, i.e. the cost to process\n");
		printf("a single authenticated packet.\n");
	} else {
		printf("authdelay in the configuration file\n");
		printf("real authentication delay: %.6f\n",
		    rtime/(double)loops);
		printf("authentication delay in CPU time: %.6f\n",
		    vtime/(double)loops);
		printf("\nThe CPU delay is probably the best bet for\n");
		printf("authdelay in the configuration file\n");
	}
	exit(0);
}


/*
 * dodelay - do the delay measurement
 */
static void
dodelay(loops)
	int loops;
{
	double vtime1, rtime1, vtime2, rtime2;
	register int loopcount;
	/*
	 *  If we're attempting to compute the cost of an auth2crypt()
	 *  for first compute the total cost, then compute the
	 *  cost of only doing the first step, auth1crypt().  What 
	 *  remains is the cost of auth2crypt.
	 */
	loopcount = loops;
	(void) gettimeofday(&tstart, (struct timezone *)0);
	(void) getrusage(RUSAGE_SELF, &rstart);
	
	while (loopcount-- > 0) {
		auth1crypt((loops & 0xf) + domd5, pkt, 48);
		L_ADDUF(&dummy1, dummy3);
		auth2crypt((loops & 0xf) + domd5, pkt, 48);
	}
	
	(void) getrusage(RUSAGE_SELF, &rend);
	(void) gettimeofday(&tend, (struct timezone *)0);
	
	subtime(&tstart, &tend, &rtime1);
#ifdef FAKE_RUSAGE
	vtime1 = (rend.tms_utime - rstart.tms_utime) * 1.0 / HZ;
#else
	subtime(&rstart.ru_utime, &rend.ru_utime, &vtime1);
#endif
printf("Time for full encryptions is %f rusage %f real\n", vtime1, rtime1);
	loopcount = loops;
	(void) gettimeofday(&tstart, (struct timezone *)0);
	(void) getrusage(RUSAGE_SELF, &rstart);

	while (loopcount-- > 0) {
		auth1crypt((loops & 0xf) + domd5, pkt, 48);
	}

	(void) getrusage(RUSAGE_SELF, &rend);
	(void) gettimeofday(&tend, (struct timezone *)0);

	subtime(&tstart, &tend, &rtime2);
#ifdef FAKE_RUSAGE
	vtime2 = (rend.tms_utime - rstart.tms_utime) * 1.0 / HZ;
#else
	subtime(&rstart.ru_utime, &rend.ru_utime, &vtime2);
#endif

printf("Time for auth1crypt is %f rusage %f real\n", vtime2, rtime2);
	vtime = vtime1 - vtime2;
	rtime = rtime1 - rtime2;
}


/*
 * docheap - do the cost measurement the cheap way
 */
static void
docheap(loops)
	register int loops;
{

	(void) authhavekey(3 + domd5);

	(void) gettimeofday(&tstart, (struct timezone *)0);
	(void) getrusage(RUSAGE_SELF, &rstart);

	while (loops-- > 0) {
		auth1crypt(3 + domd5, pkt, 48);
		L_ADDUF(&dummy1, dummy3);
		auth2crypt(3 + domd5, pkt, 48);
		(void) authdecrypt(3 + domd5, pkt, 48);
	}

	(void) getrusage(RUSAGE_SELF, &rend);
	(void) gettimeofday(&tend, (struct timezone *)0);

	subtime(&tstart, &tend, &rtime);
#ifdef FAKE_RUSAGE
	vtime = (rend.tms_utime - rstart.tms_utime) * 1.0 / HZ;
#else
	subtime(&rstart.ru_utime, &rend.ru_utime, &vtime);
#endif
}


/*
 * docost - do the cost measurement
 */
static void
docost(loops)
	register int loops;
{

	(void) gettimeofday(&tstart, (struct timezone *)0);
	(void) getrusage(RUSAGE_SELF, &rstart);

	while (loops-- > 0) {
		auth1crypt((loops & 0xf) + domd5, pkt, 48);
		L_ADDUF(&dummy1, dummy3);
		auth2crypt((loops & 0xf) + domd5, pkt, 48);
		(void) authdecrypt(((loops+1) & 0xf) + domd5, pkt, 48);
	}

	(void) getrusage(RUSAGE_SELF, &rend);
	(void) gettimeofday(&tend, (struct timezone *)0);

	subtime(&tstart, &tend, &rtime);
#ifdef FAKE_RUSAGE
	vtime = (rend.tms_utime - rstart.tms_utime) * 1.0 / HZ;
#else
	subtime(&rstart.ru_utime, &rend.ru_utime, &vtime);
#endif
}


/*
 * subtime - subtract two struct timevals, return double result
 */
static void
subtime(tvs, tve, res)
	struct timeval *tvs, *tve;
	double *res;
{
	LONG sec;
	LONG usec;

	sec = tve->tv_sec - tvs->tv_sec;
	usec = tve->tv_usec - tvs->tv_usec;

	if (usec < 0) {
		usec += 1000000;
		sec--;
	}

	*res = (double)sec + (double)usec/1000000.;
	return;
}
