/* cbc.c: This file contains the encryption routines for the ed line editor */
/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Matt Bishop of Dartmouth College.
 *
 * The United States Government has rights in this work pursuant
 * to contract no. NAG 2-680 between the National Aeronautics and
 * Space Administration and Dartmouth College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)cbc.c	5.5 (Berkeley) 6/27/91";
#endif /* not lint */

/* Author: Matt Bishop
 *	   Department of Mathematics and Computer Science
 *	   Dartmouth College
 *	   Hanover, NH  03755
 * Email:  Matt.Bishop@dartmouth.edu
 *	   ...!decvax!dartvax!Matt.Bishop
 *
 * See Technical Report PCS-TR91-158, Department of Mathematics and Computer
 * Science, Dartmouth College, for a detailed description of the implemen-
 * tation and differences between it and Sun's.  The DES is described in
 * FIPS PUB 46, and the modes in FIPS PUB 81 (see either the manual page
 * or the technical report for a complete reference).
 */

#include <errno.h>
#include <pwd.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "ed.h"

/*
 * Define a divisor for rand() that yields a uniform distribution in the
 * range 0-255.
 */
#define	RAND_DIV (((unsigned) RAND_MAX + 1) >> 8)

/*
 * BSD and System V systems offer special library calls that do
 * block moves and fills, so if possible we take advantage of them
 */
#define	MEMCPY(dest,src,len)	memcpy((dest),(src),(len))
#define	MEMZERO(dest,len)	memset((dest), 0, (len))

/* Hide the calls to the primitive encryption routines. */
#define	DES_KEY(buf) \
	if (des_setkey(buf)) \
		err("des_setkey");
#define	DES_XFORM(buf) \
	if (des_cipher(buf, buf, 0L, (inverse ? -1 : 1))) \
		err("des_cipher");

/*
 * read/write - no error checking
 */
#define	READ(buf, n, fp)	fread(buf, sizeof(char), n, fp)
#define WRITE(buf, n, fp)	fwrite(buf, sizeof(char), n, fp)

/*
 * some things to make references easier
 */
typedef char Desbuf[8];
#define	CHAR(x,i)	(x[i])
#define	UCHAR(x,i)	(x[i])
#define	BUFFER(x)	(x)
#define	UBUFFER(x)	(x)

/*
 * global variables and related macros
 */

enum { 					/* encrypt, decrypt, authenticate */
	MODE_ENCRYPT, MODE_DECRYPT, MODE_AUTHENTICATE
} mode = MODE_ENCRYPT;

Desbuf ivec;				/* initialization vector */
Desbuf pvec;				/* padding vector */
char bits[] = {				/* used to extract bits from a char */
	'\200', '\100', '\040', '\020', '\010', '\004', '\002', '\001'
};
int pflag;				/* 1 to preserve parity bits */

char des_buf[8];		/* shared buffer for desgetc/desputc */
int des_ct = 0;			/* count for desgetc/desputc */
int des_n = 0;			/* index for desputc/desgetc */


/* desinit: initialize DES */
void
desinit()
{
#ifdef DES
	int i;

	des_ct = des_n = 0;

	/* initialize the initialization vctor */
	MEMZERO(ivec, 8);

	/* intialize the padding vector */
	srand((unsigned) time((time_t *) 0));
	for (i = 0; i < 8; i++)
		CHAR(pvec, i) = (char) (rand()/RAND_DIV);
#endif
}


/* desgetc: return next char in an encrypted file */
desgetc(fp)
	FILE *fp;
{
#ifdef DES
	if (des_n >= des_ct) {
		des_n = 0;
		des_ct = cbcdec(des_buf, fp);
	}
	return (des_ct > 0) ? des_buf[des_n++] : EOF;
#endif
}


/* desputc: write a char to an encrypted file; return char written */
desputc(c, fp)
	int c;
	FILE *fp;
{
#ifdef DES
	if (des_n == sizeof des_buf) {
		des_ct = cbcenc(des_buf, des_n, fp);
		des_n = 0;
	}
	return (des_ct >= 0) ? (des_buf[des_n++] = c) : EOF;
#endif
}


/* desflush: flush an encrypted file's output; return status */
desflush(fp)
	FILE *fp;
{
#ifdef DES
	if (des_n == sizeof des_buf) {
		des_ct = cbcenc(des_buf, des_n, fp);
		des_n = 0;
	}
	return (des_ct >= 0 && cbcenc(des_buf, des_n, fp) >= 0) ? 0 : EOF;
#endif
}

#ifdef DES
/*
 * get keyword from tty or stdin
 */
getkey()
{
	register char *p;		/* used to obtain the key */
	Desbuf msgbuf;			/* I/O buffer */

	/*
	 * get the key
	 */
	if (*(p = getpass("Enter key: "))) {

		/*
		 * copy it, nul-padded, into the key area
		 */
		cvtkey(BUFFER(msgbuf), p);
		MEMZERO(p, _PASSWORD_LEN);
		makekey(msgbuf);
		MEMZERO(msgbuf, sizeof msgbuf);
		return 1;
	}
	return 0;
}


extern char errmsg[];

/*
 * print a warning message and, possibly, terminate
 */
void
err(s)
	char *s;		/* the message */
{
	(void)sprintf(errmsg, "%s", s ? s : strerror(errno));
}

/*
 * map a hex character to an integer
 */
tobinhex(c, radix)
	int c;			/* char to be converted */
	int radix;		/* base (2 to 16) */
{
	switch(c) {
	case '0':		return(0x0);
	case '1':		return(0x1);
	case '2':		return(radix > 2 ? 0x2 : -1);
	case '3':		return(radix > 3 ? 0x3 : -1);
	case '4':		return(radix > 4 ? 0x4 : -1);
	case '5':		return(radix > 5 ? 0x5 : -1);
	case '6':		return(radix > 6 ? 0x6 : -1);
	case '7':		return(radix > 7 ? 0x7 : -1);
	case '8':		return(radix > 8 ? 0x8 : -1);
	case '9':		return(radix > 9 ? 0x9 : -1);
	case 'A': case 'a':	return(radix > 10 ? 0xa : -1);
	case 'B': case 'b':	return(radix > 11 ? 0xb : -1);
	case 'C': case 'c':	return(radix > 12 ? 0xc : -1);
	case 'D': case 'd':	return(radix > 13 ? 0xd : -1);
	case 'E': case 'e':	return(radix > 14 ? 0xe : -1);
	case 'F': case 'f':	return(radix > 15 ? 0xf : -1);
	}
	/*
	 * invalid character
	 */
	return(-1);
}

/*
 * convert the key to a bit pattern
 */
void
cvtkey(obuf, ibuf)
	char *obuf;			/* bit pattern */
	char *ibuf;			/* the key itself */
{
	register int i, j;		/* counter in a for loop */
	int nbuf[64];			/* used for hex/key translation */

	/*
	 * leading '0x' or '0X' == hex key
	 */
	if (ibuf[0] == '0' && (ibuf[1] == 'x' || ibuf[1] == 'X')) {
		ibuf = &ibuf[2];
		/*
		 * now translate it, bombing on any illegal hex digit
		 */
		for (i = 0; ibuf[i] && i < 16; i++)
			if ((nbuf[i] = tobinhex((int) ibuf[i], 16)) == -1)
				err("bad hex digit in key");
		while (i < 16)
			nbuf[i++] = 0;
		for (i = 0; i < 8; i++)
			obuf[i] =
			    ((nbuf[2*i]&0xf)<<4) | (nbuf[2*i+1]&0xf);
		/* preserve parity bits */
		pflag = 1;
		return;
	}
	/*
	 * leading '0b' or '0B' == binary key
	 */
	if (ibuf[0] == '0' && (ibuf[1] == 'b' || ibuf[1] == 'B')) {
		ibuf = &ibuf[2];
		/*
		 * now translate it, bombing on any illegal binary digit
		 */
		for (i = 0; ibuf[i] && i < 16; i++)
			if ((nbuf[i] = tobinhex((int) ibuf[i], 2)) == -1)
				err("bad binary digit in key");
		while (i < 64)
			nbuf[i++] = 0;
		for (i = 0; i < 8; i++)
			for (j = 0; j < 8; j++)
				obuf[i] = (obuf[i]<<1)|nbuf[8*i+j];
		/* preserve parity bits */
		pflag = 1;
		return;
	}
	/*
	 * no special leader -- ASCII
	 */
	(void)strncpy(obuf, ibuf, 8);
}

/*****************
 * DES FUNCTIONS *
 *****************/
/*
 * This sets the DES key and (if you're using the deszip version)
 * the direction of the transformation.  This uses the Sun
 * to map the 64-bit key onto the 56 bits that the key schedule
 * generation routines use: the old way, which just uses the user-
 * supplied 64 bits as is, and the new way, which resets the parity
 * bit to be the same as the low-order bit in each character.  The
 * new way generates a greater variety of key schedules, since many
 * systems set the parity (high) bit of each character to 0, and the
 * DES ignores the low order bit of each character.
 */
void
makekey(buf)
	Desbuf buf;				/* key block */
{
	register int i, j;			/* counter in a for loop */
	register int par;			/* parity counter */

	/*
	 * if the parity is not preserved, flip it
	 */
	if (!pflag) {
		for (i = 0; i < 8; i++) {
			par = 0;
			for (j = 1; j < 8; j++)
				if ((bits[j]&UCHAR(buf, i)) != 0)
					par++;
			if ((par&01) == 01)
				UCHAR(buf, i) = UCHAR(buf, i)&0177;
			else
				UCHAR(buf, i) = (UCHAR(buf, i)&0177)|0200;
		}
	}

	DES_KEY(UBUFFER(buf));
}


/*
 * This encrypts using the Cipher Block Chaining mode of DES
 */
cbcenc(msgbuf, n, fp)
	char *msgbuf;
	int n;
	FILE *fp;
{
	int inverse = 0;	/* 0 to encrypt, 1 to decrypt */

	/*
	 * do the transformation
	 */
	if (n == 8) {
		for (n = 0; n < 8; n++)
			CHAR(msgbuf, n) ^= CHAR(ivec, n);
		DES_XFORM(UBUFFER(msgbuf));
		MEMCPY(BUFFER(ivec), BUFFER(msgbuf), 8);
		return WRITE(BUFFER(msgbuf), 8, fp);
	}
	/*
	 * at EOF or last block -- in either case, the last byte contains
	 * the character representation of the number of bytes in it
	 */
/*
	MEMZERO(msgbuf +  n, 8 - n);
*/
	/*
	 *  Pad the last block randomly
	 */
	(void)MEMCPY(BUFFER(msgbuf + n), BUFFER(pvec), 8 - n);
	CHAR(msgbuf, 7) = n;
	for (n = 0; n < 8; n++)
		CHAR(msgbuf, n) ^= CHAR(ivec, n);
	DES_XFORM(UBUFFER(msgbuf));
	return WRITE(BUFFER(msgbuf), 8, fp);
}

/*
 * This decrypts using the Cipher Block Chaining mode of DES
 */
cbcdec(msgbuf, fp)
	char *msgbuf;		/* I/O buffer */
	FILE *fp;			/* input file descriptor */
{
	Desbuf ibuf;	/* temp buffer for initialization vector */
	register int n;		/* number of bytes actually read */
	register int c;		/* used to test for EOF */
	int inverse = 1;	/* 0 to encrypt, 1 to decrypt */

	if ((n = READ(BUFFER(msgbuf), 8, fp)) == 8) {
		/*
		 * do the transformation
		 */
		MEMCPY(BUFFER(ibuf), BUFFER(msgbuf), 8);
		DES_XFORM(UBUFFER(msgbuf));
		for (c = 0; c < 8; c++)
			UCHAR(msgbuf, c) ^= UCHAR(ivec, c);
		MEMCPY(BUFFER(ivec), BUFFER(ibuf), 8);
		/*
		 * if the last one, handle it specially
		 */
		if ((c = fgetc(fp)) == EOF) {
			n = CHAR(msgbuf, 7);
			if (n < 0 || n > 7) {
				err("decryption failed (block corrupted)");
				return EOF;
			}
		} else
			(void)ungetc(c, fp);
		return n;
	}
	if (n > 0)
		err("decryption failed (incomplete block)");
	else if (n < 0)
		err("cannot read file");
	return EOF;
}
#endif	/* DES */
