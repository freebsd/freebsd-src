/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Baptiste Daroussin <bapt@FreeBSD.org>
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

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern int main_quotedprintable(int, char *[]);

static int
hexval(int c)
{
	if ('0' <= c && c <= '9')
		return c - '0';
	return (10 + c - 'A');
}


static int
decode_char(const char *s)
{
	return (16 * hexval(toupper(s[1])) + hexval(toupper(s[2])));
}


static void
decode_quoted_printable(const char *body, FILE *fpo)
{
	while (*body != '\0') {
		switch (*body) {
		case '=':
			if (strlen(body) < 2) {
				fputc(*body, fpo);
				break;
			}

			if (body[1] == '\r' && body[2] == '\n') {
				body += 2;
				break;
			}
			if (body[1] == '\n') {
				body++;
				break;
			}
			if (strchr("0123456789ABCDEFabcdef", body[1]) == NULL) {
				fputc(*body, fpo);
				break;
			}
			if (strchr("0123456789ABCDEFabcdef", body[2]) == NULL) {
				fputc(*body, fpo);
				break;
			}
			fputc(decode_char(body), fpo);
			body += 2;
			break;
		default:
			fputc(*body, fpo);
			break;
		}
		body++;
	}
}

static void
encode_quoted_printable(const char *body, FILE *fpo)
{
	const char *end = body + strlen(body);
	size_t linelen = 0;
	char prev = '\0';

	while (*body != '\0') {
		if (linelen == 75) {
			fputs("=\r\n", fpo);
			linelen = 0;
		}
		if (!isascii(*body) ||
		    *body == '=' ||
		    (*body == '.' && body + 1 < end &&
		      (body[1] == '\n' || body[1] == '\r'))) {
			fprintf(fpo, "=%02X", (unsigned char)*body);
			linelen += 2;
			prev = *body;
		} else if (*body < 33 && *body != '\n') {
			if ((*body == ' ' || *body == '\t') &&
			    body + 1 < end &&
			    (body[1] != '\n' && body[1] != '\r')) {
				fputc(*body, fpo);
				prev = *body;
			} else {
				fprintf(fpo, "=%02X", (unsigned char)*body);
				linelen += 2;
				prev = '_';
			}
		} else if (*body == '\n') {
			if (prev == ' ' || prev == '\t') {
				fputc('=', fpo);
			}
			fputc('\n', fpo);
			linelen = 0;
			prev = 0;
		} else {
			fputc(*body, fpo);
			prev = *body;
		}
		body++;
		linelen++;
	}
}

static void
qp(FILE *fp, FILE *fpo, bool encode)
{
	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	void (*codec)(const char *line, FILE *f);

	codec = encode ? encode_quoted_printable : decode_quoted_printable ;

	while ((linelen = getline(&line, &linecap, fp)) > 0)
		codec(line, fpo);
	free(line);
}

static void
usage(void)
{
	fprintf(stderr,
	   "usage: bintrans qp [-u] [-o outputfile] [file name]\n");
}

int
main_quotedprintable(int argc, char *argv[])
{
	int i;
	bool encode = true;
	FILE *fp = stdin;
	FILE *fpo = stdout;

	for (i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			case 'o':
				if (++i >= argc) {
					fprintf(stderr, "qp: -o requires a file name.\n");
					exit(EXIT_FAILURE);
				}
				fpo = fopen(argv[i], "w");
				if (fpo == NULL) {
					perror(argv[i]);
					exit(EXIT_FAILURE);
				}
				break;
			case 'u':
				encode = false;
				break;
			default:
				usage();
				exit(EXIT_FAILURE);
			}
		} else {
			fp = fopen(argv[i], "r");
			if (fp == NULL) {
				perror(argv[i]);
				exit(EXIT_FAILURE);
			}
		}
	}
	qp(fp, fpo, encode);

	return (EXIT_SUCCESS);
}
