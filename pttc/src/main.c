/*
 * Copyright (c) 2013-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "pttc.h"

#include "pt_cpu.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/* Prints this tools version number and libipt version number on stdout.  */
static void version(const char *prog)
{
	struct pt_version v;

	v = pt_library_version();
	printf("%s-%d.%d.%d%s / libipt-%" PRIu8 ".%" PRIu8 ".%" PRIu32 "%s\n",
	       prog, PT_VERSION_MAJOR, PT_VERSION_MINOR, PT_VERSION_BUILD,
	       PT_VERSION_EXT, v.major, v.minor, v.build, v.ext);
}

/* Prints usage information to stdout.  */
static void help(const char *prog)
{
	printf("usage: %s [<options>] <pttfile>\n\n"
	       "options:\n"
	       "  --help|-h                this text.\n"
	       "  --version                display version information and exit.\n"
	       "  --cpu none|auto|f/m[/s]  set cpu to the given value and encode according to:\n"
	       "                             none     spec (default)\n"
	       "                             auto     current cpu\n"
	       "                             f/m[/s]  family/model[/stepping]\n"
	       "  <pttfile>                the annotated yasm input file.\n",
	       prog);
}

int main(int argc, char *argv[])
{
	struct pttc_options options;
	const char *prog;
	int errcode, i;

	prog = argv[0];
	memset(&options, 0, sizeof(options));

	for (i = 1; i < argc;) {
		const char *arg;

		arg = argv[i++];

		if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
			help(prog);
			return 0;
		}
		if (strcmp(arg, "--version") == 0) {
			version(prog);
			return 0;
		}
		if (strcmp(arg, "--cpu") == 0) {
			arg = argv[i++];

			if (strcmp(arg, "auto") == 0) {
				errcode = pt_cpu_read(&options.cpu);
				if (errcode < 0) {
					fprintf(stderr,
						"%s: error reading cpu: %s.\n",
						prog,
						pt_errstr(pt_errcode(errcode)));
					return 1;
				}
				continue;
			}

			if (strcmp(arg, "none") == 0) {
				memset(&options.cpu, 0, sizeof(options.cpu));
				continue;
			}

			errcode = pt_cpu_parse(&options.cpu, arg);
			if (errcode < 0) {
				fprintf(stderr,
					"%s: cpu must be specified as f/m[/s].\n",
					prog);
				return 1;
			}
			continue;
		}

		if (arg[0] == '-') {
			fprintf(stderr, "%s: unrecognized option '%s'.\n",
				prog, arg);
			return 1;
		}

		if (options.pttfile) {
			fprintf(stderr,
				"%s: only one pttfile can be specified.\n",
				prog);
			return 1;
		}
		options.pttfile = arg;
	}

	if (!options.pttfile) {
		fprintf(stderr, "%s: no pttfile specified.\n", prog);
		fprintf(stderr, "Try '%s -h' for more information.\n", prog);
		return 1;
	}

	return pttc_main(&options);
}
