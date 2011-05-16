/*-
 * Copyright (c) 2007 Diomidis Spinellis. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <syslog.h>
#include <time.h>

#define KASSERT(val, msg) assert(val)

typedef u_int32_t comp_t;

#define AHZ 1000000

#include "convert.c"

static int nerr;

union cf {
	comp_t c;
	float f;
};

static void
check_result(const char *name, float expected, union cf v)
{
	double eps;

	eps = fabs(expected - v.f) / expected;
	if (eps > FLT_EPSILON) {
		printf("Error in %s\n", name);
		printf("Got      0x%08x %12g\n", v.c, v.f);
		v.f = expected;
		printf("Expected 0x%08x %12g (%.15lg)\n", v.c, v.f, expected);
		printf("Epsilon=%lg, rather than %g\n", eps, FLT_EPSILON);
		nerr++;
	}
}

int
main(int argc, char *argv[])
{
	union cf v;
	long l;
	int i, end;
	struct timeval tv;

	if (argc == 2) {
		/* Loop test */
		end = atoi(argv[1]);
		for (i = 0; i < end; i++) {
			tv.tv_sec = random();
			tv.tv_usec = (random() % 1000000);
			v.c = encode_timeval(tv);
			check_result("encode_timeval",
			    (float)tv.tv_sec * AHZ + tv.tv_usec, v);
			l = random();
			v.c = encode_long(l);
			check_result("encode_long", l, v);
		}
	} else if (argc == 3) {
		/* Single-value timeval/long test */
		tv.tv_sec = atol(argv[1]);
		tv.tv_usec = atol(argv[2]);
		v.c = encode_timeval(tv);
		check_result("encode_timeval",
		    (float)tv.tv_sec * AHZ + tv.tv_usec, v);
		v.c = encode_long(tv.tv_sec);
		check_result("encode_long", tv.tv_sec, v);
	} else {
		fprintf(stderr, "usage:\n%s repetitions\n%s sec usec\n",
		    argv[0], argv[0]);
		return (1);
	}
	return (nerr);
}
