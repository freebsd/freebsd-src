/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Arm Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/errno.h>

#include <machine/atomic.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>

#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* #define DEBUG_BUFRING */

#ifdef DEBUG_BUFRING
static void
panic(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	exit(1);
}
#endif

static void
critical_enter(void)
{
}

static void
critical_exit(void)
{
}

#include "../../../sys/sys/buf_ring.h"

#define	PROD_ITERATIONS	100000000

static enum {
	CT_UNKNOWN,
	CT_MC,
	CT_MC_MT,
	CT_SC,
	CT_PEEK,
	CT_PEEK_CLEAR,
} cons_type = CT_UNKNOWN;

static unsigned int prod_count;

static struct buf_ring *br;
static _Atomic bool prod_done = false;
static _Atomic int prod_done_count = 0;
static _Atomic size_t total_cons_count = 0;

static uint64_t *mt_seen;

static void *
producer(void *arg)
{
	int id, rv;

	id = (int)(uintptr_t)arg;

	for (size_t i = 0; i < PROD_ITERATIONS;) {
		rv = buf_ring_enqueue(br, (void *)(i * prod_count + 1 + id));
		if (rv == 0) {
			i++;
		}
	}
	if ((unsigned int)atomic_fetch_add(&prod_done_count, 1) ==
	    (prod_count - 1))
		atomic_store(&prod_done, true);

	return (NULL);
}

static void *
consumer(void *arg)
{
	void *val;
	size_t *max_vals;
	size_t consume_count, curr;
	int id;

	(void)arg;

	max_vals = calloc(prod_count, sizeof(*max_vals));
	assert(max_vals != NULL);

	/* Set the initial value to be the expected value */
	for (unsigned int i = 1; i < prod_count; i++) {
		max_vals[i] = (int)(i - prod_count);
	}

	consume_count = 0;
	while (!atomic_load(&prod_done) || !buf_ring_empty(br)) {
		switch(cons_type) {
		case CT_MC:
		case CT_MC_MT:
			val = buf_ring_dequeue_mc(br);
			break;
		case CT_SC:
			val = buf_ring_dequeue_sc(br);
			break;
		case CT_PEEK:
			val = buf_ring_peek(br);
			if (val != NULL)
				buf_ring_advance_sc(br);
			break;
		case CT_PEEK_CLEAR:
			val = buf_ring_peek_clear_sc(br);
			if (val != NULL)
				buf_ring_advance_sc(br);
			break;
		case CT_UNKNOWN:
			__unreachable();
		}
		if (val != NULL) {
			consume_count++;
			curr = (size_t)(uintptr_t)val;
			id = curr % prod_count;
			if (cons_type != CT_MC_MT) {
				if (curr != max_vals[id] + prod_count)
					printf("Incorrect val: %zu Expect: %zu "
					    "Difference: %zd\n", curr,
					    max_vals[id] + prod_count,
					    curr - max_vals[id] - prod_count);
			} else {
				size_t idx, bit;

				idx = ((size_t)(uintptr_t)val - 1) /
				    (sizeof(*mt_seen) * NBBY);
				bit = ((size_t)(uintptr_t)val - 1) %
				    (sizeof(*mt_seen) * NBBY);

				if (atomic_testandset_64(&mt_seen[idx], bit))
					printf("Repeat ID: %zx\n", (size_t)(uintptr_t)val);
			}

			max_vals[id] = (uintptr_t)val;
		}
	}

	atomic_fetch_add(&total_cons_count, consume_count);

	for (unsigned int i = 0; i < prod_count; i++)
		printf("max[%d] = %zu\n", i, max_vals[i]);

	return (NULL);
}

static struct option longopts[] = {
	{ "buf-size",	required_argument,	NULL,	'b'	},
	{ "cons-type",	required_argument,	NULL,	'c'	},
	{ "prod-count",	required_argument,	NULL,	'p'	},
	{ "help",	no_argument,		NULL,	'h'	},
	{ NULL,		0,			NULL,	 0	},
};

static void
usage(void)
{
	errx(1, "test --cons-type=<mc|mc-mt|sc|peek|peek-clear> --prod-count=<prod thread count> [--buf-size=<buf_ring size>]");
}

static uint32_t
next_power_of_2(uint32_t x)
{
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;
	return (x);
}

int
main(int argc, char *argv[])
{
	pthread_t *prod;
	pthread_t cons[2];
	const char *errstr;
	uint32_t size;
	int ch, ret;

	size = 0;
	while ((ch = getopt_long(argc, argv, "bf:", longopts, NULL)) != -1) {
		switch(ch) {
		case 'b':
			errstr = NULL;
			size = strtonum(optarg, 1, UINT_MAX, &errstr);
			if (errstr != NULL) {
				errx(1, "--bufsize=%s: %s", optarg, errstr);
			}
			if (!powerof2(size)) {
				errx(1, "--bufsize needs a power of 2 size");
			}
			break;
		case 'c':
			if (strcmp(optarg, "mc") == 0) {
				cons_type = CT_MC;
			} else if (strcmp(optarg, "mc-mt") == 0) {
				cons_type = CT_MC_MT;
			} else if (strcmp(optarg, "sc") == 0) {
				cons_type = CT_SC;
			} else if (strcmp(optarg, "peek") == 0) {
				cons_type = CT_PEEK;
			} else if (strcmp(optarg, "peek-clear") == 0) {
				cons_type = CT_PEEK_CLEAR;
			} else {
				errx(1, "Unknown --cons-type: %s", optarg);
			}
			break;
		case 'p':
			errstr = NULL;
			prod_count = strtonum(optarg, 1, UINT_MAX, &errstr);
			if (errstr != NULL) {
				errx(1, "--prod-count=%s: %s", optarg, errstr);
			}
			break;
		case 'h':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (cons_type == CT_UNKNOWN)
		errx(1, "No cons-type set");

	if (prod_count == 0)
		errx(1, "prod-count is not set");

	if (size == 0)
		size = next_power_of_2(prod_count);

	if (cons_type == CT_MC_MT) {
		size_t entries;

		entries = (size_t)PROD_ITERATIONS * prod_count;
		entries = roundup2(entries, sizeof(*mt_seen));
		mt_seen = calloc(entries / (sizeof(*mt_seen) * NBBY),
		    sizeof(*mt_seen));
	}

	br = buf_ring_alloc(size);

	ret = pthread_create(&cons[0], NULL, consumer, NULL);
	assert(ret == 0);
	if (cons_type == CT_MC_MT) {
		ret = pthread_create(&cons[1], NULL, consumer, NULL);
		assert(ret == 0);
	}

	prod = calloc(prod_count, sizeof(*prod));
	assert(prod != NULL);
	for (unsigned i = 0; i < prod_count; i++) {
		ret = pthread_create(&prod[i], NULL, producer,
		    (void *)(uintptr_t)i);
		assert(ret == 0);
	}

	for (unsigned int i = 0; i < prod_count; i++) {
		ret = pthread_join(prod[i], NULL);
		assert(ret == 0);
	}
	ret = pthread_join(cons[0], NULL);
	assert(ret == 0);
	if (cons_type == CT_MC_MT) {
		ret = pthread_join(cons[1], NULL);
		assert(ret == 0);
	}

	printf("Expected: %zu\n", (size_t)PROD_ITERATIONS * prod_count);
	printf("Received: %zu\n", total_cons_count);

	buf_ring_free(br);

	return (0);
}
