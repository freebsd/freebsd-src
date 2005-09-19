/*-
 * Copyright (c) 2005 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <errno.h>
#include <libgeom.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int verbose;

static void
usage()
{
	fprintf(stdout, "usage: %s [-v] param ...\n", getprogname());
	exit(1);
}

static int
parse(char *arg, char **param, char **value)
{
	char *e;

	*param = arg;
	e = strchr(arg, '=');
	if (e != NULL) {
		*e = '\0';
		*value = e + 1;
	} else
		*value = NULL;
	return (0);
}

static int
add_int32(struct gctl_req *req, char *arg)
{
	char *param, *nptr, *endptr;
	uint32_t value;
	int error;

	error = parse(arg, &param, &nptr);
	if (error)
		return (error);
	if (nptr == NULL)
		return (EINVAL);
	value = strtol(nptr, &endptr, 0);
	if (*endptr)
		return (EINVAL);
	gctl_ro_param(req, param, sizeof(value), &value);
	return (0);
}

static int
add_string(struct gctl_req *req, char *arg)
{
	char *param, *value;
	int error;

	error = parse(arg, &param, &value);
	if (error)
		return (error);
	if (value == NULL)
		return (EINVAL);

	gctl_ro_param(req, param, -1, value);
	return (0);
}

int main(int argc, char *argv[])
{
	struct gctl_req *req;
	const char *s;
	int c;

	req = gctl_get_handle();
	gctl_ro_param(req, "class", -1, "GPT");

	while ((c = getopt(argc, argv, "4:s:v")) != -1) {
		switch (c) {
		case '4':	/* uint32_t */
			add_int32(req, optarg);
			break;
		case 's':	/* string */
			add_string(req, optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
			break;
		}
	}
	if (argc != optind)
		usage();

	if (verbose)
		gctl_dump(req, stdout);

	s = gctl_issue(req);
	if (s != NULL)
		printf("FAIL %s\n", s);
	else
		printf("PASS\n");
	gctl_free(req);
	return (0);
}
