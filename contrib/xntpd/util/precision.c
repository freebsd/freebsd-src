#include <sys/types.h>
#include <sys/time.h>

#define	DEFAULT_SYS_PRECISION	-99

int default_get_precision();

int
main() {
	printf("log2(precision) = %d\n", default_get_precision());
	return 0;
}

/* Find the precision of the system clock by watching how the current time
 * changes as we read it repeatedly.
 *
 * struct timeval is only good to 1us, which may cause problems as machines
 * get faster, but until then the logic goes:
 *
 * If a machine has precision (i.e. accurate timing info) > 1us, then it will
 * probably use the "unused" low order bits as a counter (to force time to be
 * a strictly increaing variable), incrementing it each time any process
 * requests the time [[ or maybe time will stand still ? ]].
 *
 * SO: the logic goes:
 *
 *      IF      the difference from the last time is "small" (< MINSTEP)
 *      THEN    this machine is "counting" with the low order bits
 *      ELIF    this is not the first time round the loop
 *      THEN    this machine *WAS* counting, and has now stepped
 *      ELSE    this machine has precision < time to read clock
 *
 * SO: if it exits on the first loop, assume "full accuracy" (1us)
 *     otherwise, take the log2(observered difference, rounded UP)
 *
 * MINLOOPS > 1 ensures that even if there is a STEP between the initial call
 * and the first loop, it doesn't stop too early.
 * Making it even greater allows MINSTEP to be reduced, assuming that the
 * chance of MINSTEP-1 other processes getting in and calling gettimeofday
 * between this processes's calls.
 * Reducing MINSTEP may be necessary as this sets an upper bound for the time
 * to actually call gettimeofday.
 */

#define	DUSECS	1000000
#define	HUSECS	(1024 * 1024)
#define	MINSTEP	5	/* some systems increment uS on each call */
			/* Don't use "1" as some *other* process may read too*/
			/*We assume no system actually *ANSWERS* in this time*/
#define	MAXLOOPS HUSECS	/* Assume precision < .1s ! */

int default_get_precision()
{
	struct timeval tp;
	struct timezone tzp;
	long last;
	int i;
	long diff;
	long val;
	int minsteps = 2;	/* need at least this many steps */

	gettimeofday(&tp, &tzp);
	last = tp.tv_usec;
	for (i = - --minsteps; i< MAXLOOPS; i++) {
		gettimeofday(&tp, &tzp);
		diff = tp.tv_usec - last;
		if (diff < 0) diff += DUSECS;
		if (diff > MINSTEP) if (minsteps-- <= 0) break;
		last = tp.tv_usec;
	}

	printf("precision calculation given %dus after %d loop%s\n",
		diff, i, (i==1) ? "" : "s");

	diff = (diff *3)/2;
        if (i >= MAXLOOPS)      diff = 1; /* No STEP, so FAST machine */
	if (i == 0)             diff = 1; /* time to read clock >= precision */
	for (i=0, val=HUSECS; val>0; i--, val >>= 1) if (diff >= val) return i;
	return DEFAULT_SYS_PRECISION /* Something's BUST, so lie ! */;
}

