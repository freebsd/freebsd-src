#ifndef lint
static char sccsid[] = 	"@(#)mp.c	2.1 88/08/15 4.0 RPCSRC Copyr 1988 Sun Micro";
#endif
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/*
 * These routines add hexadecimal functionality to the multiple-precision
 * library.
 */
#include <stdio.h>
#include <mp.h>

void mfree();

/*
 * Convert hex digit to binary value
 */
static int
xtoi(c)
	char c;
{
	if (c >= '0' && c <= '9') {
		return(c - '0');
	} else if (c >= 'a' && c <= 'f') {
		return(c - 'a' + 10);
	} else {
		return(-1);
	}
}

/*
 * Convert hex key to MINT key
 */
MINT *
xtom(key)
	char *key;
{
	int digit;
	MINT *m = itom(0);
	MINT *d;
	MINT *sixteen;
 	sixteen = itom(16);
	for (; *key; key++) {
		digit = xtoi(*key);
		if (digit < 0) {
			return(NULL);
		}
		d = itom(digit);
		mult(m,sixteen,m);
		madd(m,d,m);
		mfree(d);
	}
	mfree(sixteen);
	return(m);
}
static char
itox(d)
	short d;
{
	d &= 15;
	if (d < 10) {
		return('0' + d);
	} else {
		return('a' - 10 + d);
	}
}
/*
 * Convert MINT key to hex key
 */
char *
mtox(key)
	MINT *key;
{
	MINT *m = itom(0);
	MINT *zero = itom(0);
	short r;
	char *p;
	char c;
	char *s;
	char *hex;
	int size;
#define BASEBITS	(8*sizeof(short) - 1)
 	if (key->len >= 0) {
		size = key->len;
	} else {
		size = -key->len;
	}
	hex = malloc((unsigned) ((size * BASEBITS + 3)) / 4 + 1);
	if (hex == NULL) {
		return(NULL);
	}
	move(key,m);
	p = hex;
	do {
		sdiv(m,16,m,&r);
		*p++ = itox(r);
	} while (mcmp(m,zero) != 0);
	mfree(m);
	mfree(zero);
 	*p = 0;
	for (p--, s = hex; s < p; s++, p--) {
		c = *p;
		*p = *s;
		*s = c;
	}
	return(hex);
}
/*
 * Deallocate a multiple precision integer
 */
void
mfree(a)
	MINT *a;
{
	xfree(a);
	free((char *)a);
}

