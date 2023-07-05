/* $FreeBSD$ */
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Mellanox Technologies. All rights reserved.
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <err.h>

#include <sys/sysctl.h>

#include "../kern_testfrwk.h"
#include "../callout_test.h"

static struct kern_test kern_test = {
	.num_threads = 1,
	.tot_threads_running = 1,
};

static struct callout_test callout_test = {
	.number_of_callouts = 1,
	.test_number = 0,
};

static void
usage()
{
	fprintf(stderr, "Usage: runtest -n <testname> -j <numthreads> [ -c <numcallouts> -t <testnumber> ]\n");
	exit(0);
}

int
main(int argc, char **argv)
{
	static const char options[] = "n:j:c:t:h";
	int ch;

	while ((ch = getopt(argc, argv, options)) != -1) {
		switch (ch) {
		case 'n':
			strlcpy(kern_test.name, optarg, sizeof(kern_test.name));
			break;
		case 'j':
			kern_test.num_threads =
			    kern_test.tot_threads_running = atoi(optarg);
			break;
		case 'c':
			callout_test.number_of_callouts = atoi(optarg);
			break;
		case 't':
			callout_test.test_number = atoi(optarg);
			break;
		default:
			usage();
			break;
		}
	}

	if (kern_test.name[0] == 0)
		usage();
	if (strcmp(kern_test.name, "callout_test") == 0)
		memcpy(kern_test.test_options, &callout_test, sizeof(callout_test));

	if (sysctlbyname("kern.testfrwk.runtest", NULL, NULL, &kern_test, sizeof(kern_test)) != 0)
		errx(1, "Test '%s' could not be started", kern_test.name);
	return (0);
}
