/*-
 * Copyright (c) 1999, 2000, 2001 Robert N. M. Watson
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
#include <unistd.h>
#include <vis.h>

void
usage(void)
{

	fprintf(stderr, "getextattr [-s] [attrnamespace] [attrname] "
	    "[filename ...]\n");
	exit(-1);
}

extern char	*optarg;
extern int	optind;

#define BUFSIZE	2048

int
main(int argc, char *argv[])
{
	struct iovec	iov_buf;
	char	*attrname;
	char	buf[BUFSIZE];
	char	visbuf[BUFSIZE*4];
	int	error, i, arg_counter, attrnamespace;
	int	ch;

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
	if (error) {
		perror(argv[0]);
		return (-1);
	}
	attrname = argv[1];

	argc--;
	argv++;

	iov_buf.iov_base = buf;
	iov_buf.iov_len = BUFSIZE;

	for (arg_counter = 1; arg_counter < argc; arg_counter++) {
		error = extattr_get_file(argv[arg_counter], attrnamespace,
		    attrname, &iov_buf, 1);

		if (error == -1)
			perror(argv[arg_counter]);
		else {
			if (flag_as_string) {
				strvisx(visbuf, buf, error, VIS_SAFE
				    | VIS_WHITE);
				if (flag_reverse) {
					printf("%s ", visbuf);
					printf("%s\n", argv[arg_counter]);
				} else {
					printf("%s:", argv[arg_counter]);
					printf(" \"%s\"\n", visbuf);
				}
			} else {
				printf("%s:", argv[arg_counter]);
				for (i = 0; i < error; i++)
					if (i % 16 == 0)
						printf("\n  %02x ", buf[i]);
					else if (i % 8 == 0)
						printf("   %02x ", buf[i]);
					else
						printf("%02x ", buf[i]);
				printf("\n");
			}
		}
	}

	return (0);
}
