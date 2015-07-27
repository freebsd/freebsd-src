/*-
 * Copyright (c) 2015 Baptiste Daroussin <bapt@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/sbuf.h>

#include <ctype.h>
#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xlocale.h>

static bool
parse_id(FILE *fp, struct sbuf *buf, locale_t l)
{
	int c;
	bool isid = false;
	bool subversion = false;

	sbuf_putc(buf, '$');
	while ((c = fgetc(fp)) != EOF) {
		sbuf_putc(buf, c);
		if (!isid) {
			if (c == '$') {
				sbuf_clear(buf);
				sbuf_putc(buf, '$');
				continue;
			}
			if (c == ':') {
				 c = fgetc(fp);
				 /* accept :: for subversion compatibility */
				 if (c == ':') {
					subversion = true;
					sbuf_putc(buf, c);
					c = fgetc(fp);
				}
				if (c == ' ') {
					sbuf_putc(buf, c);
					isid = true;
					continue;
				}
				return (false);
			}

			if (!isalpha_l(c, l))
				return (false);
		} else {
			if (c == '\n')
				return (false);
			if (c == '$') {
				sbuf_finish(buf);
				/* should end with a space */
				c = sbuf_data(buf)[sbuf_len(buf) - 2];
				if (!subversion) {
					if (c != ' ')
						return (0);
				} else if (subversion) {
					if (c != ' ' && c != '#')
						return (0);
				}
				printf("     %s\n", sbuf_data(buf));
				return (true);
			}
		}
	}

	return (false);
}

static int
scan(FILE *fp, const char *name, bool quiet)
{
	int c;
	bool hasid = false;
	struct sbuf *id = sbuf_new_auto();
	locale_t l;

	l = newlocale(LC_ALL_MASK, "C", NULL);

	if (name != NULL)
		printf("%s:\n", name);

	while ((c = fgetc(fp)) != EOF) {
		if (c == '$') {
			sbuf_clear(id);
			if (parse_id(fp, id, l))
				hasid = true;
		}
	}
	sbuf_delete(id);
	freelocale(l);

	if (!hasid) {
		if (!quiet)
			fprintf(stderr, "%s warning: no id keywords in %s\n",
			    getprogname(), name ? name : "standard input");

		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
	bool quiet = false;
	int ch, i;
	int ret = EXIT_SUCCESS;
	FILE *fp;

	while ((ch = getopt(argc, argv, "qV")) != -1) {
		switch (ch) {
		case 'q':
			quiet = true;
			break;
		case 'V':
			/* Do nothing, compat with GNU rcs's ident */
			return (EXIT_SUCCESS);
		default:
			errx(EXIT_FAILURE, "usage: %s [-q] [-V] [file...]",
			    getprogname());
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		return (scan(stdin, NULL, quiet));

	for (i = 0; i < argc; i++) {
		fp = fopen(argv[i], "r");
		if (fp == NULL) {
			warn("%s", argv[i]);
			ret = EXIT_FAILURE;
			continue;
		}
		if (scan(fp, argv[i], quiet) != EXIT_SUCCESS)
			ret = EXIT_FAILURE;
		fclose(fp);
	}

	return (ret);
}
