/****************************************************************
Copyright 1990, 1993 by AT&T Bell Laboratories and Bellcore.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T Bell Laboratories or
Bellcore or any of their entities not be used in advertising or
publicity pertaining to distribution of the software without
specific, written prior permission.

AT&T and Bellcore disclaim all warranties with regard to this
software, including all implied warranties of merchantability
and fitness.  In no event shall AT&T or Bellcore be liable for
any special, indirect or consequential damages or any damages
whatsoever resulting from loss of use, data or profits, whether
in an action of contract, negligence or other tortious action,
arising out of or in connection with the use or performance of
this software.
****************************************************************/

/* Put strings representing decimal floating-point numbers
 * into canonical form: always have a decimal point or
 * exponent field; if using an exponent field, have the
 * number before it start with a digit and decimal point
 * (if the number has more than one digit); only have an
 * exponent field if it saves space.
 *
 * Arrange that the return value, rv, satisfies rv[0] == '-' || rv[-1] == '-' .
 */

#include "sysdep.h"

 char *
cds(s, z0)
 char *s, *z0;
{
	int ea, esign, et, i, k, nd = 0, sign = 0, tz;
	char c, *z;
	char ebuf[24];
	long ex = 0;
	static char etype[Table_size], *db;
	static int dblen = 64;

	if (!db) {
		etype['E'] = 1;
		etype['e'] = 1;
		etype['D'] = 1;
		etype['d'] = 1;
		etype['+'] = 2;
		etype['-'] = 3;
		db = Alloc(dblen);
		}

	while((c = *s++) == '0');
	if (c == '-')
		{ sign = 1; c = *s++; }
	else if (c == '+')
		c = *s++;
	k = strlen(s) + 2;
	if (k >= dblen) {
		do dblen <<= 1;
			while(k >= dblen);
		free(db);
		db = Alloc(dblen);
		}
	if (etype[(unsigned char)c] >= 2)
		while(c == '0') c = *s++;
	tz = 0;
	while(c >= '0' && c <= '9') {
		if (c == '0')
			tz++;
		else {
			if (nd)
				for(; tz; --tz)
					db[nd++] = '0';
			else
				tz = 0;
			db[nd++] = c;
			}
		c = *s++;
		}
	ea = -tz;
	if (c == '.') {
		while((c = *s++) >= '0' && c <= '9') {
			if (c == '0')
				tz++;
			else {
				if (tz) {
					ea += tz;
					if (nd)
						for(; tz; --tz)
							db[nd++] = '0';
					else
						tz = 0;
					}
				db[nd++] = c;
				ea++;
				}
			}
		}
	if (et = etype[(unsigned char)c]) {
		esign = et == 3;
		c = *s++;
		if (et == 1) {
			if(etype[(unsigned char)c] > 1) {
				if (c == '-')
					esign = 1;
				c = *s++;
				}
			}
		while(c >= '0' && c <= '9') {
			ex = 10*ex + (c - '0');
			c = *s++;
			}
		if (esign)
			ex = -ex;
		}
	switch(c) {
		case 0:
			break;
#ifndef VAX
		case 'i':
		case 'I':
			Fatal("Overflow evaluating constant expression.");
		case 'n':
		case 'N':
			Fatal("Constant expression yields NaN.");
#endif
		default:
			Fatal("unexpected character in cds.");
		}
	ex -= ea;
	if (!nd) {
		if (!z0)
			z0 = mem(4,0);
		strcpy(z0, "-0.");
		sign = 0;
		}
	else if (ex > 2 || ex + nd < -2) {
		sprintf(ebuf, "%ld", ex + nd - 1);
		k = strlen(ebuf) + nd + 3;
		if (nd > 1)
			k++;
		if (!z0)
			z0 = mem(k,0);
		z = z0;
		*z++ = '-';
		*z++ = *db;
		if (nd > 1) {
			*z++ = '.';
			for(k = 1; k < nd; k++)
				*z++ = db[k];
			}
		*z++ = 'e';
		strcpy(z, ebuf);
		}
	else {
		k = (int)(ex + nd);
		i = nd + 3;
		if (k < 0)
			i -= k;
		else if (ex > 0)
			i += ex;
		if (!z0)
			z0 = mem(i,0);
		z = z0;
		*z++ = '-';
		if (ex >= 0) {
			for(k = 0; k < nd; k++)
				*z++ = db[k];
			while(--ex >= 0)
				*z++ = '0';
			*z++ = '.';
			}
		else {
			for(i = 0; i < k;)
				*z++ = db[i++];
			*z++ = '.';
			while(++k <= 0)
				*z++ = '0';
			while(i < nd)
				*z++ = db[i++];
			}
		*z = 0;
		}
	return sign ? z0 : z0+1;
	}
