/*
Copyright (c) 1991 Bell Communications Research, Inc. (Bellcore)

Permission to use, copy, modify, and distribute this material
for any purpose and without fee is hereby granted, provided
that the above copyright notice and this permission notice
appear in all copies, and that the name of Bellcore not be
used in advertising or publicity pertaining to this
material without the specific, prior written permission
of an authorized representative of Bellcore.  BELLCORE
MAKES NO REPRESENTATIONS ABOUT THE ACCURACY OR SUITABILITY
OF THIS MATERIAL FOR ANY PURPOSE.  IT IS PROVIDED "AS IS",
WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES.
*/
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern int	main_quotedprintable(int, char *[]);

static int
PendingBoundary(char *s, char **Boundaries, int *BoundaryCt)
{
	int i;
	size_t len;

	if (s[0] != '-' || s[1] != '-')
		return (0);

	for (i = 0; i < *BoundaryCt; ++i) {
		len = strlen(Boundaries[i]);
		if (strncmp(s, Boundaries[i], len) == 0) {
			if (s[len] == '-' && s[len + 1] == '-')
				*BoundaryCt = i;
			return (1);
		}
	}
	return (0);
}

#define basis_hex "0123456789ABCDEF"
static const char index_hex[128] = {
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	 0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,
	-1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	-1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
};

/* The following version generated complaints on Solaris. */
/* #define hexchar(c)  (((c) < 0 || (c) > 127) ? -1 : index_hex[(c)])  */
/*  Since we're no longer ever calling it with anything signed, this should work: */
#define hexchar(c)  (((c) > 127) ? -1 : index_hex[(c)])

static void
toqp(FILE *infile, FILE *outfile)
{
	int c, ct = 0, prevc = 255;

	while ((c = getc(infile)) != EOF) {
		if ((c < 32 && (c != '\n' && c != '\t'))
			 || (c == '=')
			 || (c >= 127)
			 /* Following line is to avoid single periods alone on lines,
			   which messes up some dumb smtp implementations, sigh... */
			 || (ct == 0 && c == '.')) {
			putc('=', outfile);
			putc(basis_hex[c >> 4], outfile);
			putc(basis_hex[c & 0xF], outfile);
			ct += 3;
			prevc = 'A'; /* close enough */
		} else if (c == '\n') {
			if (prevc == ' ' || prevc == '\t') {
				putc('=', outfile); /* soft & hard lines */
				putc(c, outfile);
			}
			putc(c, outfile);
			ct = 0;
			prevc = c;
		} else {
			if (c == 'F' && prevc == '\n') {
				/* HORRIBLE but clever hack suggested by MTR for sendmail-avoidance */
				c = getc(infile);
				if (c == 'r') {
					c = getc(infile);
					if (c == 'o') {
						c = getc(infile);
						if (c == 'm') {
							c = getc(infile);
							if (c == ' ') {
								/* This is the case we are looking for */
								fputs("=46rom", outfile);
								ct += 6;
							} else {
								fputs("From", outfile);
								ct += 4;
							}
						} else {
							fputs("Fro", outfile);
							ct += 3;
						}
					} else {
						fputs("Fr", outfile);
						ct += 2;
					}
				} else {
					putc('F', outfile);
					++ct;
				}
				ungetc(c, infile);
				prevc = 'x'; /* close enough -- printable */
			} else { /* END horrible hack */
				putc(c, outfile);
				++ct;
				prevc = c;
			}
		}
		if (ct > 72) {
			putc('=', outfile);
			putc('\n', outfile);
			ct = 0;
			prevc = '\n';
		}
	}
	if (ct) {
		putc('=', outfile);
		putc('\n', outfile);
	}
}

static void
fromqp(FILE *infile, FILE *outfile, char **boundaries, int *boundaryct)
{
	int c1, c2;
	bool sawnewline = true, neednewline = false;
	/* The neednewline hack is necessary because the newline leading into
	  a multipart boundary is part of the boundary, not the data */

	while ((c1 = getc(infile)) != EOF) {
		if (sawnewline && boundaries && c1 == '-') {
			char Buf[200];
			unsigned char *s;

			ungetc(c1, infile);
			fgets(Buf, sizeof(Buf), infile);
			if (boundaries
				 && Buf[0] == '-'
				 && Buf[1] == '-'
				 && PendingBoundary(Buf, boundaries, boundaryct)) {
				return;
			}
			/* Not a boundary, now we must treat THIS line as q-p, sigh */
			if (neednewline) {
				putc('\n', outfile);
				neednewline = false;
			}
			for (s = (unsigned char *)Buf; *s; ++s) {
				if (*s == '=') {
					if (*++s == 0)
						break;
					if (*s == '\n') {
						/* ignore it */
						sawnewline = true;
					} else {
						c1 = hexchar(*s);
						if (*++s == 0)
							break;
						c2 = hexchar(*s);
						putc(c1 << 4 | c2, outfile);
					}
				} else {
					putc(*s, outfile);
				}
			}
		} else {
			if (neednewline) {
				putc('\n', outfile);
				neednewline = false;
			}
			if (c1 == '=') {
				sawnewline = false;
				c1 = getc(infile);
				if (c1 == '\n') {
					/* ignore it */
					sawnewline = true;
				} else {
					c2 = getc(infile);
					c1 = hexchar(c1);
					c2 = hexchar(c2);
					putc(c1 << 4 | c2, outfile);
					if (c2 == '\n')
						sawnewline = true;
				}
			} else {
				if (c1 == '\n') {
					sawnewline = true;
					neednewline = true;
				} else {
					sawnewline = false;
					putc(c1, outfile);
				}
			}
		}
	}
	if (neednewline) {
		putc('\n', outfile);
		neednewline = false;
	}
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
	if (encode)
		toqp(fp, fpo);
	else
		fromqp(fp, fpo, NULL, 0);
	return (0);
}
