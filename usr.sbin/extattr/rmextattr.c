/*-
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * Copyright (c) 2002 Poul-Henning Kamp.
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning
 * Kamp and Network Associates Laboratories, the Security Research Division
 * of Network Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/extattr.h>

#include <libgen.h>
#include <libutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>
#include <err.h>
#include <errno.h>

static enum { EADUNNO, EAGET, EASET, EARM, EALS } what = EADUNNO;

static void __dead2
usage(void) 
{

	switch (what) {
	case EAGET:
		fprintf(stderr, "usage: getextattr [-fhqsx] attrnamespace");
		fprintf(stderr, " attrname filename ...\n");
		exit(-1);
	case EASET:
		fprintf(stderr, "usage: setextattr [-fhq] attrnamespace");
		fprintf(stderr, " attrname attrvalue filename ...\n");
		exit(-1);
	case EARM:
		fprintf(stderr, "usage: rmextattr [-fhq] attrnamespace");
		fprintf(stderr, " attrname filename ...\n");
		exit(-1);
	case EALS:
		fprintf(stderr, "usage: lsextattr [-fhq] attrnamespace");
		fprintf(stderr, " filename ...\n");
		exit(-1);
	case EADUNNO:
	default:
		fprintf(stderr, "usage: (getextattr|lsextattr|rmextattr");
		fprintf(stderr, "|setextattr)\n");
		exit (-1);
	}
}

static void
mkbuf(char **buf, int *oldlen, int newlen)
{

	if (*oldlen >= newlen)
		return;
	if (*buf != NULL)
		free(*buf);
	*buf = malloc(newlen);
	if (*buf == NULL)
		err(1, "malloc");
	*oldlen = newlen;
	return;
}

int
main(int argc, char *argv[])
{
	char	*buf, *visbuf, *p;

	const char *options, *attrname;
	int	 buflen, visbuflen, ch, error, i, arg_counter, attrnamespace,
		 minargc;

	int	flag_force = 0;
	int	flag_nofollow = 0;
	int	flag_quiet = 0;
	int	flag_string = 0;
	int	flag_hex = 0;

	visbuflen = buflen = 0;
	visbuf = buf = NULL;

	p = basename(argv[0]);
	if (p == NULL)
		p = argv[0];
	if (!strcmp(p, "getextattr")) {
		what = EAGET;
		options = "fhqsx";
		minargc = 3;
	} else if (!strcmp(p, "setextattr")) {
		what = EASET;
		options = "fhq";
		minargc = 4;
	} else if (!strcmp(p, "rmextattr")) {
		what = EARM;
		options = "fhq";
		minargc = 3;
	} else if (!strcmp(p, "lsextattr")) {
		what = EALS;
		options = "fhq";
		minargc = 2;
	} else {
		usage();
	}

	while ((ch = getopt(argc, argv, options)) != -1) {
		switch (ch) {
		case 'f':
			flag_force = 1;
			break;
		case 'h':
			flag_nofollow = 1;
			break;
		case 'q':
			flag_quiet = 1;
			break;
		case 's':
			flag_string = 1;
			break;
		case 'x':
			flag_hex = 1;
			break;
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < minargc)
		usage();

	error = extattr_string_to_namespace(argv[0], &attrnamespace);
	if (error)
		err(-1, argv[0]);
	argc--; argv++;

	if (what == EALS) {
		attrname = "";
	} else {
		attrname = argv[0];
		argc--; argv++;
	}

	if (what == EASET) {
		mkbuf(&buf, &buflen, strlen(argv[0]) + 1);
		strcpy(buf, argv[0]);
		argc--; argv++;
	}

	for (arg_counter = 0; arg_counter < argc; arg_counter++) {
		switch (what) {
		case EARM:
			if (flag_nofollow)
				error = extattr_delete_link(argv[arg_counter],
				    attrnamespace, attrname);
			else
				error = extattr_delete_file(argv[arg_counter],
				    attrnamespace, attrname);
			if (error >= 0)
				continue;
			break;
		case EASET:
			if (flag_nofollow)
				error = extattr_set_link(argv[arg_counter],
				    attrnamespace, attrname, buf, strlen(buf));
			else
				error = extattr_set_file(argv[arg_counter],
				    attrnamespace, attrname, buf, strlen(buf));
			if (error >= 0)
				continue;
			break;
		case EALS:
		case EAGET:
			if (flag_nofollow)
				error = extattr_get_link(argv[arg_counter],
				    attrnamespace, attrname, NULL, 0);
			else
				error = extattr_get_file(argv[arg_counter],
				    attrnamespace, attrname, NULL, 0);
			if (error < 0)
				break;
			mkbuf(&buf, &buflen, error);
			if (flag_nofollow)
				error = extattr_get_link(argv[arg_counter],
				    attrnamespace, attrname, buf, buflen);
			else
				error = extattr_get_file(argv[arg_counter],
				    attrnamespace, attrname, buf, buflen);
			if (error < 0)
				break;
			if (!flag_quiet)
				printf("%s\t", argv[arg_counter]);
			if (what == EALS) {
				for (i = 0; i < error; i += buf[i] + 1)
				    printf("%s%*.*s", i ? "\t" : "",
					buf[i], buf[i], buf + i + 1);
				printf("\n");
				continue;
			}
			if (flag_string) {
				mkbuf(&visbuf, &visbuflen, error * 4 + 1);
				strvisx(visbuf, buf, error,
				    VIS_SAFE | VIS_WHITE);
				printf("\"%s\"\n", visbuf);
				continue;
			} else if (flag_hex) {
				for (i = 0; i < error; i++)
					printf("%s%02x", i ? " " : "",
					    buf[i]);
				printf("\n");
				continue;
			} else {
				fwrite(buf, buflen, 1, stdout);
				printf("\n");
				continue;
			}
		default:
			break;
		}
		if (!flag_quiet) 
			warn("%s: failed", argv[arg_counter]);
		if (flag_force)
			continue;
		return(1);
	}
	return (0);
}
