/* $FreeBSD$ */

/*
 * Little program to dump the crypto statistics block and, optionally,
 * zero all the stats or just the timing stuff.
 */
#include <stdio.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <crypto/cryptodev.h>

static void
printt(const char* tag, struct cryptotstat *ts)
{
	uint64_t avg, min, max;

	if (ts->count == 0)
		return;
	avg = (1000000000LL*ts->acc.tv_sec + ts->acc.tv_nsec) / ts->count;
	min = 1000000000LL*ts->min.tv_sec + ts->min.tv_nsec;
	max = 1000000000LL*ts->max.tv_sec + ts->max.tv_nsec;
	printf("%16.16s: avg %6llu ns : min %6llu ns : max %7llu ns [%u samps]\n",
		tag, avg, min, max, ts->count);
}

int
main(int argc, char *argv[])
{
	struct cryptostats stats;
	size_t slen;

	slen = sizeof (stats);
	if (sysctlbyname("kern.crypto_stats", &stats, &slen, NULL, NULL) < 0)
		err(1, "kern.cryptostats");

	if (argc > 1 && strcmp(argv[1], "-z") == 0) {
		bzero(&stats.cs_invoke, sizeof (stats.cs_invoke));
		bzero(&stats.cs_done, sizeof (stats.cs_done));
		bzero(&stats.cs_cb, sizeof (stats.cs_cb));
		bzero(&stats.cs_finis, sizeof (stats.cs_finis));
		stats.cs_invoke.min.tv_sec = 10000;
		stats.cs_done.min.tv_sec = 10000;
		stats.cs_cb.min.tv_sec = 10000;
		stats.cs_finis.min.tv_sec = 10000;
		if (sysctlbyname("kern.crypto_stats", NULL, NULL, &stats, sizeof (stats)) < 0)
			err(1, "kern.cryptostats");
		exit(0);
	}
	if (argc > 1 && strcmp(argv[1], "-Z") == 0) {
		bzero(&stats, sizeof (stats));
		stats.cs_invoke.min.tv_sec = 10000;
		stats.cs_done.min.tv_sec = 10000;
		stats.cs_cb.min.tv_sec = 10000;
		stats.cs_finis.min.tv_sec = 10000;
		if (sysctlbyname("kern.crypto_stats", NULL, NULL, &stats, sizeof (stats)) < 0)
			err(1, "kern.cryptostats");
		exit(0);
	}


	printf("%u symmetric crypto ops (%u errors, %u times driver blocked)\n"
		, stats.cs_ops, stats.cs_errs, stats.cs_blocks);
	printf("%u key ops (%u errors, %u times driver blocked)\n"
		, stats.cs_kops, stats.cs_kerrs, stats.cs_kblocks);
	printf("%u crypto dispatch thread activations\n", stats.cs_intrs);
	printf("%u crypto return thread activations\n", stats.cs_rets);
	if (stats.cs_invoke.count) {
		printf("\n");
		printt("dispatch->invoke", &stats.cs_invoke);
		printt("invoke->done", &stats.cs_done);
		printt("done->cb", &stats.cs_cb);
		printt("cb->finis", &stats.cs_finis);
	}
	return 0;
}
