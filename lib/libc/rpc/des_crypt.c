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
 * des_crypt.c, DES encryption library routines
 * Copyright (C) 1986, Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <rpc/des_crypt.h>
#include <rpc/des.h>

#ifndef lint
/* from: static char sccsid[] = "@(#)des_crypt.c	2.2 88/08/10 4.0 RPCSRC; from 1.13 88/02/08 SMI"; */
static const char rcsid[] = "$FreeBSD$";
#endif

static int common_crypt	__P(( char *, char *, register unsigned, unsigned, struct desparams * ));
int (*__des_crypt_LOCAL)() = 0;
extern _des_crypt_call __P(( char *, int, struct desparams * ));
/*
 * Copy 8 bytes
 */
#define COPY8(src, dst) { \
	register char *a = (char *) dst; \
	register char *b = (char *) src; \
	*a++ = *b++; *a++ = *b++; *a++ = *b++; *a++ = *b++; \
	*a++ = *b++; *a++ = *b++; *a++ = *b++; *a++ = *b++; \
}
 
/*
 * Copy multiple of 8 bytes
 */
#define DESCOPY(src, dst, len) { \
	register char *a = (char *) dst; \
	register char *b = (char *) src; \
	register int i; \
	for (i = (int) len; i > 0; i -= 8) { \
		*a++ = *b++; *a++ = *b++; *a++ = *b++; *a++ = *b++; \
		*a++ = *b++; *a++ = *b++; *a++ = *b++; *a++ = *b++; \
	} \
}

/*
 * CBC mode encryption
 */
int
cbc_crypt(key, buf, len, mode, ivec)
	char *key;
	char *buf;
	unsigned len;
	unsigned mode;
	char *ivec;	
{
	int err;
	struct desparams dp;

#ifdef BROKEN_DES
	dp.UDES.UDES_buf = buf;
	dp.des_mode = ECB;
#else
	dp.des_mode = CBC;
#endif
	COPY8(ivec, dp.des_ivec);
	err = common_crypt(key, buf, len, mode, &dp);
	COPY8(dp.des_ivec, ivec);
	return(err);
}


/*
 * ECB mode encryption
 */
int
ecb_crypt(key, buf, len, mode)
	char *key;
	char *buf;
	unsigned len;
	unsigned mode;
{
	struct desparams dp;

#ifdef BROKEN_DES
	dp.UDES.UDES_buf = buf;
	dp.des_mode = CBC;
#else
	dp.des_mode = ECB;
#endif
	return(common_crypt(key, buf, len, mode, &dp));
}



/*
 * Common code to cbc_crypt() & ecb_crypt()
 */
static int
common_crypt(key, buf, len, mode, desp)	
	char *key;	
	char *buf;
	register unsigned len;
	unsigned mode;
	register struct desparams *desp;
{
	register int desdev;

	if ((len % 8) != 0 || len > DES_MAXDATA) {
		return(DESERR_BADPARAM);
	}
	desp->des_dir =
		((mode & DES_DIRMASK) == DES_ENCRYPT) ? ENCRYPT : DECRYPT;

	desdev = mode & DES_DEVMASK;
	COPY8(key, desp->des_key);
	/* 
	 * software
	 */
	if (__des_crypt_LOCAL != NULL) {
		if (!__des_crypt_LOCAL(buf, len, desp)) {
			return (DESERR_HWERROR);
		}
	} else {
		if (!_des_crypt_call(buf, len, desp)) {
			return (DESERR_HWERROR);
		}
	}
	return(desdev == DES_SW ? DESERR_NONE : DESERR_NOHWDEVICE);
}
