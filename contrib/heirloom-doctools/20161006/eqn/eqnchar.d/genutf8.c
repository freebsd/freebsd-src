/*
 * Generate a table of UTF-8 characters.
 *
 * Copyright (C) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Public Domain
 *
 * Sccsid @(#)genutf8.c	1.1 (gritter) 9/13/05
 */
#ifdef	EUC
#include <locale.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>
#include <stdio.h>

const char *const ctl[] = {
	"nul",
	"soh",
	"stx",
	"etx",
	"eot",
	"enq",
	"ack",
	"bel",
	"bs",
	"ht",
	"nl",
	"vt",
	"np",
	"cr",
	"so",
	"si",
	"dle",
	"dc1",
	"dc2",
	"dc3",
	"dc4",
	"nak",
	"syn",
	"etb",
	"can",
	"em",
	"sub",
	"esc",
	"fs",
	"gs",
	"rs",
	"us"
};

int
main(void)
{
	int	wc;
	int	i, n;

	if (setlocale(LC_CTYPE, "en_US.utf8") == NULL)
		if (setlocale(LC_CTYPE, "en_US.UTF-8") == NULL)
			return 1;
	for (wc = 0; wc <= 0xffff; wc++) {
		if ((wc&017) == 0)
			printf("U+%04X  ", wc);
		if (wc < 040)
			n = printf("%s", ctl[wc]);
		else if (wc == 0177)
			n = printf("del");
		else if (wc >= 0200 && wc < 0240)
			n = printf("CTL");
		else if (iswprint(wc)) {
			if ((n = wcwidth(wc)) == 0)
				n = 1;
			printf("%lc", wc);
		} else
			n = 0;
		for (i = n; i < 4; i++)
			putchar(' ');
		if (((wc+1)&017) == 0)
			putchar('\n');
	}
	return 0;
}
#else	/* !EUC */
int
main(void)
{
	return 1;
}
#endif	/* !EUC */
