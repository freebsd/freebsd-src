/*
 * Copyright 1987 by MIT Student Information Processing Board
 * For copyright info, see Copyright.SIPB.
 *
 *	$Id: et_name.c,v 1.2 1994/07/19 19:21:27 g89r4222 Exp $
 */

#include "error_table.h"

static char copyright[] = "Copyright 1987 by MIT Student Information Processing Board";

char *malloc();

char *
error_table_name(num)
	int num;
{
	register int ch;
	register int i;
	register char *buf, *p;

	/* num = aa aaa abb bbb bcc ccc cdd ddd d?? ??? ??? */
	buf = malloc(5);
	p = buf;
	num >>= ERRCODE_RANGE;
	/* num = ?? ??? ??? aaa aaa bbb bbb ccc ccc ddd ddd */
	num &= 077777777;
	/* num = 00 000 000 aaa aaa bbb bbb ccc ccc ddd ddd */
	for (i = 0; i < 5; i++) {
		ch = (num >> 24-6*i) & 077;
		if (ch == 0)
			continue;
		else if (ch < 27)
			*p++ = ch - 1 + 'A';
		else if (ch < 53)
			*p++ = ch - 27 + 'a';
		else if (ch < 63)
			*p++ = ch - 53 + '0';
		else		/* ch == 63 */
			*p++ = '_';
	}
	return(buf);
}

