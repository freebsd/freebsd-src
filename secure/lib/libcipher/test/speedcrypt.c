#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#include <stdio.h>

int		keep_going, count, alternate, seconds;
struct rusage	prior, now;

void
finish()
{
	keep_going = 0;
}


main(int argc, char *argv[])
{
	struct itimerval itv;
	u_long		msecs, key1[8], key2[8];
	char		*k1, *k2;

	if (argc < 2 || sscanf(argv[1], "%d", &seconds) != 1)
		seconds = 20;

	if (argc < 3 || sscanf(argv[2], "%d", &alternate) != 1)
		alternate = 0;

	printf ("Running crypt%s for %d seconds of vtime...\n",
		alternate ? " with alternate keys" : "", seconds);

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

	k1 = (char *) key1;
	k2 = (char *) key2;
	strcpy(k1, "fredfredfredfredfred");
	strcpy(k2, "joejoejoejoejoejoejo");

	if (alternate)
		for (count = 0; keep_going; count++)
		{
#if defined(LONGCRYPT)
			crypt((count & 1) ? k1 : k2, "_ara.X...");
#else
			crypt((count & 1) ? k1 : k2, "eek");
#endif
		}
	else
		for (count = 0; keep_going; count++)
		{
#if defined(LONGCRYPT)
			crypt(k1, "_ara.X...");
#else
			crypt(k1, "eek");
#endif
		}

	if (getrusage(0, &now) < 0) {
		perror("getrusage");
		exit(1);
	}
	msecs = (now.ru_utime.tv_sec - prior.ru_utime.tv_sec) * 1000
	      + (now.ru_utime.tv_usec - prior.ru_utime.tv_usec) / 1000;
	printf ("\tDid %d crypt()s per second.\n", 1000 * count / msecs);
	exit(0);
}
