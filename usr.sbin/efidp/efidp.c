/*-
 * Copyright (c) 2016 Netflix, Inc.
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <ctype.h>
#include <efivar.h>
#include <efivar-dp.h>
#include <err.h>
#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* options descriptor */
static struct option longopts[] = {
	{ "format",		no_argument,		NULL,	'f' },
	{ "parse",		no_argument,		NULL,	'p' },
	{ NULL,			0,			NULL,	0 }
};


static int flag_format, flag_parse;

static void
usage(void)
{

	errx(1, "efidp [-fp]");
}

static ssize_t
read_file(int fd, void **rv) 
{
	uint8_t *retval;
	size_t len;
	off_t off;
	ssize_t red;

	len = 4096;
	off = 0;
	retval = malloc(len);
	do {
		red = read(fd, retval + off, len - off);
		off += red;
		if (red < (ssize_t)(len - off))
			break;
		len *= 2;
		retval = reallocf(retval, len);
		if (retval == NULL)
			return -1;
	} while (1);
	*rv = retval;

	return off;
}

static void
parse_args(int argc, char **argv)
{
	int ch;

	while ((ch = getopt_long(argc, argv, "fp",
		    longopts, NULL)) != -1) {
		switch (ch) {
		case 'f':
			flag_format++;
			break;
		case 'p':
			flag_parse++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc >= 1)
		usage();
	
	if (flag_parse + flag_format != 1) {
		warnx("Can only use one of -p (--parse), "
		    "and -f (--format)");
		usage();
	}
}

int
main(int argc, char **argv)
{
	void *data;
	ssize_t len;

	parse_args(argc, argv);
	len = read_file(STDIN_FILENO, &data);
	if (len == -1)
		err(1, "read");
	if (flag_format) {
		char buffer[4096];
		ssize_t fmtlen;

		fmtlen = efidp_format_device_path(buffer, sizeof(buffer),
		    (const_efidp)data, len);
		if (fmtlen > 0)
			printf("%s\n", buffer);
		free(data);
	} else if (flag_parse) {
		efidp dp;
		ssize_t dplen;
		char *str, *walker;

		dplen = 8192;
		dp = malloc(dplen);
		str = realloc(data, len + 1);
		if (str == NULL || dp == NULL)
			errx(1, "Can't allocate memory.");
		str[len] = '\0';
		walker = str;
		while (isspace(*walker))
			walker++;
		dplen = efidp_parse_device_path(walker, dp, dplen);
		if (dplen == -1)
			errx(1, "Can't parse %s", walker);
		write(STDOUT_FILENO, dp, dplen);
		free(dp);
		free(str);
	}
}
