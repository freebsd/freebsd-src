#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#include <stdio.h>

int		keep_going, count, alternate, seconds, iters;
struct rusage	prior, now;
u_long		block[3];
char		*blk;

void
finish()
{
	keep_going = 0;
}


main(int argc, char *argv[])
{
	struct itimerval itv;
	u_long	msecs;

	if (argc < 2 || sscanf(argv[1], "%d", &seconds) != 1)
		seconds = 20;

	if (argc < 3 || sscanf(argv[2], "%d", &iters) != 1)
		iters = 1;

	printf ("Running des_cipher( , , 0L, %d) for %d seconds of vtime...\n",
		iters, seconds);

	bzero(&itv, sizeof (itv));
	signal (SIGVTALRM, finish);
	itv.it_value.tv_sec = seconds;
	itv.it_value.tv_usec = 0;
	setitimer(ITIMER_VIRTUAL, &itv, NULL);

	keep_going = 1;
	if (getrusage(0, &prior) < 0) {
		perror("getrusage");
		exit(1);
	}

	blk = (char *) block;
	(void)des_setkey(blk);
	for (count = 0; keep_going; count++)
		(void) des_cipher(blk, blk, 0, iters);

	if (getrusage(0, &now) < 0) {
		perror("getrusage");
		exit(1);
	}

	msecs = (now.ru_utime.tv_sec - prior.ru_utime.tv_sec) * 1000
	      + (now.ru_utime.tv_usec - prior.ru_utime.tv_usec) / 1000;
	printf ("Did %d encryptions per second, each of %d iteration(s).\n",
		1000 * count / msecs, iters);
	printf ("\tTotal %d blocks per second.\n", (1000*iters*count)/msecs);
	exit(0);
}
