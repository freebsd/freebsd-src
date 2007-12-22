/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user or with the express written consent of
 * Sun Microsystems, Inc.
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

#if !defined(lint) && defined(SCCSIDS)
#if 0
static char sccsid[] = "@(#)generic.c 1.2 91/03/11 Copyr 1986 Sun Micro";
#endif
#endif

/*
 * Copyright (C) 1986, Sun Microsystems, Inc.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/file.h>

#include <rpc/rpc.h>
#include <rpc/key_prot.h>

#include <mp.h>
#include <stdio.h>
#include <stdlib.h>

#include "extern.h"

static void adjust(char[], char *);
static void getseed(char *, int, unsigned char *);

/*
 * Generate a seed
 */
static void
getseed(char *seed, int seedsize, unsigned char *pass)
{
	int i;

	for (i = 0; i < seedsize; i++) {
		seed[i] = (arc4random() & 0xff) ^ pass[i % 8];
	}
}

/*
 * Generate a random public/secret key pair
 */
void
genkeys(char *public, char *secret, char *pass)
{
	unsigned int i;

#   define BASEBITS (8*sizeof (short) - 1)
#	define BASE		(1 << BASEBITS)

	MINT *pk = itom(0);
	MINT *sk = itom(0);
	MINT *tmp;
	MINT *base = itom(BASE);
	MINT *root = itom(PROOT);
	MINT *modulus = xtom(HEXMODULUS);
	short r;
	unsigned short seed[KEYSIZE/BASEBITS + 1];
	char *xkey;

	getseed((char *)seed, sizeof (seed), (u_char *)pass);
	for (i = 0; i < KEYSIZE/BASEBITS + 1; i++) {
		r = seed[i] % BASE;
		tmp = itom(r);
		mult(sk, base, sk);
		madd(sk, tmp, sk);
		mfree(tmp);
	}
	tmp = itom(0);
	mdiv(sk, modulus, tmp, sk);
	mfree(tmp);
	pow(root, sk, modulus, pk);
	xkey = mtox(sk);
	adjust(secret, xkey);
	xkey = mtox(pk);
	adjust(public, xkey);
	mfree(sk);
	mfree(base);
	mfree(pk);
	mfree(root);
	mfree(modulus);
}

/*
 * Adjust the input key so that it is 0-filled on the left
 */
static void
adjust(char keyout[HEXKEYBYTES+1], char *keyin)
{
	char *p;
	char *s;

	for (p = keyin; *p; p++)
		;
	for (s = keyout + HEXKEYBYTES; p >= keyin; p--, s--) {
		*s = *p;
	}
	while (s >= keyout) {
		*s-- = '0';
	}
}
