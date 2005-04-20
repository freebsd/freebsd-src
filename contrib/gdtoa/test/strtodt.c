/****************************************************************

The author of this software is David M. Gay.

Copyright (C) 2001 by Lucent Technologies
All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the name of Lucent or any of its entities
not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission.

LUCENT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
IN NO EVENT SHALL LUCENT OR ANY OF ITS ENTITIES BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.

****************************************************************/

/* Please send bug reports to
	David M. Gay
	Bell Laboratories, Room 2C-463
	600 Mountain Avenue
	Murray Hill, NJ 07974-0636
	U.S.A.
	dmg@bell-labs.com
 */

/* Test strtod.  */

/* On stdin, read triples: d x y:
 *	d = decimal string
 *	x = high-order Hex value expected from strtod
 *	y = low-order Hex value
 * Complain about errors.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

 static int W0, W1;
 typedef union {
		double d;
		long L[2];
		} U;

 static int
process(char *fname, FILE *f)
{
	U a, b;
	char buf[2048];
	double d;
	char *s;
	int line, n;

	line = n = 0;

 top:
	while(fgets(s = buf, sizeof(buf), f)) {
		line++;
		while(*s <= ' ')
			if (!*s++)
				goto top; /* break 2 */
		if (*s == '#')
			continue;
		while(*s > ' ')
			s++;
		if (sscanf(s,"\t%lx\t%lx", &a.L[0], &a.L[1]) != 2) {
			printf("Badly formatted line %d of %s\n",
				line, fname);
			n++;
			continue;
			}
		b.d = strtod(buf,0);
		if (b.L[W0] != a.L[0] || b.L[W1] != a.L[1]) {
			n++;
			printf("Line %d of %s: got %lx %lx; expected %lx %lx\n",
				line, fname, b.L[W0], b.L[W1], a.L[0], a.L[1]);
			}
		}
	return n;
	}

 int
main(int argc, char **argv)
{
	FILE *f;
	char *prog, *s;
	int n, rc;
	U u;

	prog = argv[0];
	if (argc == 2 && !strcmp(argv[1],"-?")) {
		fprintf(stderr, "Usage: %s [file [file...]]\n"
			"\tto read data file(s) of tab-separated triples d x y with\n"
			"\t\td decimal string\n"
			"\t\tx = high-order Hex value expected from strtod\n"
			"\t\ty = low-order Hex value\n"
			"\tComplain about errors by strtod.\n"
			"\tIf no files, read triples from stdin.\n",
			prog);
		return 0;
		}

	/* determine endian-ness */

	u.d = 1.;
	W0 = u.L[0] == 0;
	W1 = 1 - W0;

	/* test */

	n = rc = 0;
	if (argc <= 1)
		n = process("<stdin>", stdin);
	else
		while(s = *++argv)
			if (f = fopen(s,"r")) {
				n += process(s, f);
				fclose(f);
				}
			else {
				rc = 2;
				fprintf(stderr, "Cannot open %s\n", s);
				}
	printf("%d bad conversions\n", n);
	if (n)
		rc |= 1;
	return rc;
	}
