/*-
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by NAI Labs,
 * the Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
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
/*
 * TrustedBSD Project - extended attribute support
 */
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/extattr.h>

#include <libutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>
#include <err.h>

#define BUFSIZE	2048

void usage(void);

void
usage(void)
{

	fprintf(stderr, "getextattr [-ls] [attrnamespace] [attrname] "
	    "[filename ...]\n");
	exit(-1);
}

int
main(int argc, char *argv[])
{
	size_t	 len;
	char	*attrname;
	char	*buf, *visbuf, *p, *pe;
	int	 ch, error, i, arg_counter, attrnamespace;

	int	flag_as_string = 0;
	int	flag_reverse = 0;

	while ((ch = getopt(argc, argv, "ls")) != -1) {
		switch (ch) {
		case 'l':
			flag_reverse = 1;
		case 's':
			flag_as_string = 1;
			break;
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 3)
		usage();

	error = extattr_string_to_namespace(argv[0], &attrnamespace);
	if (error)
		err(-1, argv[0]);
	attrname = argv[1];

	argc--;
	argv++;

	/*
	 * XXX: Note: now that EAs support querying the size, we could
	 * actually allocate a buffer of the right size, rather than
	 * truncating at BUFSIZE.
	 */
	for (arg_counter = 1; arg_counter < argc; arg_counter++) {
		len = extattr_get_file(argv[arg_counter], attrnamespace,
		    attrname, NULL, 0);
		if (len == -1) {
			perror(argv[arg_counter]);
			continue;
		}
		buf = (char *)malloc(len);
		if (buf == NULL) {
			perror("malloc");
			return (-1);
		}
		error = extattr_get_file(argv[arg_counter], attrnamespace,
		    attrname, buf, BUFSIZE);
		if (error == -1) {
			perror(argv[arg_counter]);
			free(buf);
			continue;
		}
		len = error;
		if (strlen(attrname) == 0) {	/* looking for EA names */
			visbuf = (char *)malloc(len*4);
			p = buf;
			pe = buf + len;
			printf("%s:", argv[arg_counter]);
			for (p = buf; p < pe; p += i + 1) {
				i = *p;
				strvisx(visbuf, p + 1, i,
				    VIS_SAFE | VIS_WHITE);
				printf(" \"%s\", visbuf);
			}
			free(visbuf);
			printf("\n");
		} else if (flag_as_string) {
			if (len > 0) {
				visbuf = (char *)malloc(len*4);
				if (visbuf == NULL) {
					perror("malloc");
					return (-1);
				}
				strvisx(visbuf, buf, len, VIS_SAFE | VIS_WHITE);
			} else {
				visbuf = strdup("");
			}
			if (flag_reverse) {
				printf("\"%s\" ", visbuf);
				printf("%s\n", argv[arg_counter]);
			} else {
				printf("%s:", argv[arg_counter]);
				printf(" \"%s\"\n", visbuf);
			}
			free(visbuf);
		} else {
			printf("%s:", argv[arg_counter]);
			for (i = 0; i < len; i++)
				if (i % 16 == 0)
					printf("\n  %02x ", buf[i]);
				else if (i % 8 == 0)
					printf("   %02x ", buf[i]);
				else
					printf("%02x ", buf[i]);
			printf("\n");
		}
		free(buf);
	}

	return (0);
}
