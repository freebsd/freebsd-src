/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Robert Clausecker <fuz@FreeBSD.org>
 *
 * biastest.c -- bias test for arc4random_uniform().
 *
 * The default configuration of this test has an upper bound of
 * (3/4) * UINT32_MAX, which should give a high amount of bias in
 * an incorrect implementation.  If the range reduction is
 * implemented correctly, the parameters of the statistic should
 * closely match the expected values.  If not, they'll differ.
 *
 * For memory usage reasons, we use an uchar to track the number of
 * observations per bucket.  If the number of tries is much larger
 * than upper_bound, the buckets likely overflow.  This is detected
 * by the test, but will lead to incorrect results.
 */

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void	collect_sample(unsigned char *, long long, uint32_t);
static void	analyze_sample(const unsigned char *, long long, uint32_t);

static atomic_bool complete = false;
static long long tries = 5ULL << 32;
static atomic_llong tries_done = 0;

static void
usage(const char *argv0)
{
	fprintf(stderr, "usage: %s [-n tries] [-t upper_bound]\n", argv0);
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	uint32_t threshold = 3UL << 30;
	int ch;
	unsigned char *sample;

	while (ch = getopt(argc, argv, "n:t:"), ch != EOF)
		switch (ch) {
		case 'n':
			tries = atoll(optarg);
			break;

		case 't':
			threshold = (uint32_t)atoll(optarg);
			break;

		default:
			usage(argv[0]);
		}

	if (optind != argc)
		usage(argv[0]);

	if (threshold == 0) {
		fprintf(stderr, "threshold must be between 1 and %lu\n", (unsigned long)UINT32_MAX);
		exit(EXIT_FAILURE);
	}

	sample = calloc(threshold, 1);
	if (sample == NULL) {
		perror("calloc(threshold, 1)");
		return (EXIT_FAILURE);
	}

	collect_sample(sample, tries, threshold);
	analyze_sample(sample, tries, threshold);
}

static void
progress(int signo)
{
	(void)signo;

	if (!complete) {
		fprintf(stderr, "\r%10lld of %10lld samples taken (%5.2f%% done)",
		    tries_done, tries, (tries_done * 100.0) / tries);

		signal(SIGALRM, progress);
		alarm(1);
	}
}

static void
collect_sample(unsigned char *sample, long long tries, uint32_t threshold)
{
	long long i;
	uint32_t x;
	bool overflowed = false;

	progress(SIGALRM);

	for (i = 0; i < tries; i++) {
		x = arc4random_uniform(threshold);
		tries_done++;
		assert(x < threshold);

		if (sample[x] == UCHAR_MAX) {
			if (!overflowed) {
				printf("sample table overflow, results will be incorrect\n");
				overflowed = true;
			}
		} else
			sample[x]++;
	}

	progress(SIGALRM);
	complete = true;
	fputc('\n', stderr);
}

static void
analyze_sample(const unsigned char *sample, long long tries,  uint32_t threshold)
{
	double discrepancy, average, variance, total;
	long long histogram[UCHAR_MAX + 1] = { 0 }, sum, n, median;
	uint32_t i, i_min, i_max;
	int min, max;

	printf("distribution properties:\n");

	/* find median, average, deviation, smallest, and largest bucket */
	total = 0.0;
	for (i = 0; i < threshold; i++) {
		histogram[sample[i]]++;
		total += (double)i * sample[i];
	}

	average = total / tries;

	variance = 0.0;
	median = threshold;
	n = 0;
	i_min = 0;
	i_max = 0;
	min = sample[i_min];
	max = sample[i_max];

	for (i = 0; i < threshold; i++) {
		discrepancy = i - average;
		variance += sample[i] * discrepancy * discrepancy;

		n += sample[i];
		if (median == threshold && n > tries / 2)
			median = i;

		if (sample[i] < min) {
			i_min = i;
			min = sample[i_min];
		} else if (sample[i] > max) {
			i_max = i;
			max = sample[i_max];
		}
	}

	variance /= tries;
	assert(median < threshold);

	printf("\tthreshold:	%lu\n", (unsigned long)threshold);
	printf("\tobservations:	%lld\n", tries);
	printf("\tleast common:	%lu (%d observations)\n", (unsigned long)i_min, min);
	printf("\tmost common:	%lu (%d observations)\n", (unsigned long)i_max, max);
	printf("\tmedian:		%lld (expected %lu)\n", median, (unsigned long)threshold / 2);
	printf("\taverage:	%f (expected %f)\n", average, 0.5 * (threshold - 1));
	printf("\tdeviation:	%f (expected %f)\n\n", sqrt(variance),
	    sqrt(((double)threshold * threshold - 1.0) / 12));

	/* build histogram and analyze it */
	printf("sample properties:\n");

	/* find median, average, and deviation */
	average = (double)tries / threshold;

	variance = 0.0;
	for (i = 0; i < UCHAR_MAX; i++) {
		discrepancy = i - average;
		variance += histogram[i] * discrepancy * discrepancy;
	}

	variance /= threshold;

	n = 0;
	median = UCHAR_MAX + 1;
	for (i = 0; i <= UCHAR_MAX; i++) {
		n += histogram[i];
		if (n >= threshold / 2) {
			median = i;
			break;
		}
	}

	assert(median <= UCHAR_MAX); /* unreachable */

	printf("\tmedian:		%lld\n", median);
	printf("\taverage:	%f\n", average);
	printf("\tdeviation:	%f (expected %f)\n\n", sqrt(variance), sqrt(average * (1.0 - 1.0 / threshold)));

	printf("histogram:\n");
	for (i = 0; i < 256; i++)
		if (histogram[i] != 0)
			printf("\t%3d:\t%lld\n", (int)i, histogram[i]);
}
