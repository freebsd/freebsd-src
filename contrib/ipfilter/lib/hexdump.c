/*	$NetBSD$	*/

#include <ctype.h>

#include "ipf.h"

void hexdump(out, addr, len, ascii)
FILE *out;
void *addr;
int len, ascii;
{
	FILE *fpout;
	u_char *s, *t;
	int i;

	fpout = out ? out : stdout;
	for (i = 0, s = addr; i < len; i++, s++) {
		fprintf(fpout, "%02x", *s);
		if (i % 16 == 15) {
			if (ascii != 0) {
				fputc('\t', fpout);
				for (t = s - 15; t<= s; t++)
					fputc(ISPRINT(*t) ? *t : '.', fpout);
			}
			fputc('\n', fpout);
		} else if (i % 4 == 3) {
			fputc(' ', fpout);
		}
	}
}
