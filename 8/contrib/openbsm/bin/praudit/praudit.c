/*-
 * Copyright (c) 2004-2008 Apple Inc.
 * Copyright (c) 2006 Martin Voros
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $P4: //depot/projects/trustedbsd/openbsm/bin/praudit/praudit.c#14 $
 */

/*
 * Tool used to parse audit records conforming to the BSM structure.
 */

/*
 * praudit [-lpx] [-r | -s] [-d del] [file ...]
 */

#include <bsm/libbsm.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern char	*optarg;
extern int	 optind, optopt, opterr,optreset;

static char	*del = ",";	/* Default delimiter. */
static int	 oneline = 0;
static int	 raw = 0;
static int	 shortfrm = 0;
static int	 partial = 0;
static int	 xml = 0;

static void
usage(void)
{

	fprintf(stderr, "usage: praudit [-lpx] [-r | -s] [-d del] "
	    "[file ...]\n");
	exit(1);
}

/*
 * Token printing for each token type .
 */
static int
print_tokens(FILE *fp)
{
	u_char *buf;
	tokenstr_t tok;
	int reclen;
	int bytesread;

	/* Allow tail -f | praudit to work. */
	if (partial) {
		u_char type = 0;
		/* Record must begin with a header token. */
		do {
			type = fgetc(fp);
		} while(type != AUT_HEADER32);
		ungetc(type, fp);
	}

	while ((reclen = au_read_rec(fp, &buf)) != -1) {
		bytesread = 0;
		while (bytesread < reclen) {
			/* Is this an incomplete record? */
			if (-1 == au_fetch_tok(&tok, buf + bytesread,
			    reclen - bytesread))
				break;
			if (xml)
				au_print_tok_xml(stdout, &tok, del, raw,
				    shortfrm);
			else
				au_print_tok(stdout, &tok, del, raw,
				    shortfrm);
			bytesread += tok.len;
			if (oneline) {
				if (!xml)
					printf("%s", del);
			} else
				printf("\n");
		}
		free(buf);
		if (oneline)
			printf("\n");
		fflush(stdout);
	}
	return (0);
}

int
main(int argc, char **argv)
{
	int ch;
	int i;
	FILE *fp;

	while ((ch = getopt(argc, argv, "d:lprsx")) != -1) {
		switch(ch) {
		case 'd':
			del = optarg;
			break;

		case 'l':
			oneline = 1;
			break;

		case 'p':
			partial = 1;
			break;

		case 'r':
			if (shortfrm)
				usage();	/* Exclusive from shortfrm. */
			raw = 1;
			break;

		case 's':
			if (raw)
				usage();	/* Exclusive from raw. */
			shortfrm = 1;
			break;

		case 'x':
			xml = 1;
			break;

		case '?':
		default:
			usage();
		}
	}

	if (xml)
		au_print_xml_header(stdout);

	/* For each of the files passed as arguments dump the contents. */
	if (optind == argc) {
		print_tokens(stdin);
		return (1);
	}
	for (i = optind; i < argc; i++) {
		fp = fopen(argv[i], "r");
		if ((fp == NULL) || (print_tokens(fp) == -1))
			perror(argv[i]);
		if (fp != NULL)
			fclose(fp);
	}

	if (xml)
		au_print_xml_footer(stdout);

	return (1);
}
