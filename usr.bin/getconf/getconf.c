/*
 * Copyright 2000 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "getconf.h"

static void	do_confstr(const char *name, int key);
static void	do_sysconf(const char *name, int key);
static void	do_pathconf(const char *name, int key, const char *path);

static void
usage(void)
{
	fprintf(stderr, "usage:\n"
		"\tgetconf [-v prog_model] system_var\n"
		"\tgetconf [-v prog_model] path_var pathname\n");
	exit(EX_USAGE);
}

int
main(int argc, char **argv)
{
	int c;
	const char *name, *vflag;
	int key;

	vflag = 0;

	while ((c = getopt(argc, argv, "v:")) != -1) {
		switch (c) {
		case 'v':
			vflag = optarg;
			break;

		default:
			usage();
		}
	}

	if (vflag)
		warnx("-v %s ignored", vflag);

	/* No arguments... */
	if ((name = argv[optind]) == 0)
		usage();

	if (argv[optind + 1] == 0) { /* confstr or sysconf */
		key = find_confstr(name);
		if (key >= 0) {
			do_confstr(name, key);
		} else {		
			key = find_sysconf(name);
			if (key >= 0)
				do_sysconf(name, key);
			else 
				errx(EX_USAGE,
				     "no such configuration parameter `%s'",
				     name);
		}
	} else {
		key = find_pathconf(name);
		if (key >= 0)
			do_pathconf(name, key, argv[optind + 1]);
		else
			errx(EX_USAGE,
			     "no such path configuration parameter `%s'",
			     name);
	}
	return 0;
}

static void
do_confstr(const char *name, int key)
{
	char *buf;
	size_t len;

	len = confstr(key, 0, 0);
	if (len < 0)
		err(EX_OSERR, "confstr: %s", name);
	
	if (len == 0) {
		printf("undefined\n");
	} else {
		buf = alloca(len);
		confstr(key, buf, len);
		printf("%s\n", buf);
	}
}

static void
do_sysconf(const char *name, int key)
{
	long value;

	errno = 0;
	value = sysconf(key);
	if (value == -1 && errno != 0)
		err(EX_OSERR, "sysconf: %s", name);
	else if (value == -1)
		printf("undefined\n");
	else
		printf("%ld\n", value);
}

static void
do_pathconf(const char *name, int key, const char *path)
{
	long value;

	errno = 0;
	value = pathconf(path, key);
	if (value == -1 && errno != 0)
		err(EX_OSERR, "pathconf: %s", name);
	else if (value == -1)
		printf("undefined\n");
	else
		printf("%ld\n", value);
}
