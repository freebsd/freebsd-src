/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static const char copyright[] =
"@(#) Copyright (c) 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)bdes.c	8.1 (Berkeley) 6/6/93";
#endif
#endif /* not lint */

/*
 * BDES -- DES encryption package for Berkeley Software Distribution 4.4
 * options:
 *	-a	key is in ASCII
 *	-b	use ECB (electronic code book) mode
 *	-d	invert (decrypt) input
 *	-f b	use b-bit CFB (cipher feedback) mode
 *	-F b	use b-bit CFB (cipher feedback) alternative mode
 *	-k key	use key as the cryptographic key
 *	-m b	generate a MAC of length b
 *	-o b	use b-bit OFB (output feedback) mode
 *	-p	don't reset the parity bit
 *	-v v	use v as the initialization vector (ignored for ECB)
 * note: the last character of the last block is the integer indicating
 * how many characters of that block are to be output
 *
 * Author: Matt Bishop
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/des.h>

/*
 * BSD and System V systems offer special library calls that do
 * block moves and fills, so if possible we take advantage of them
 */
#define	MEMCPY(dest,src,len)	bcopy((src),(dest),(len))
#define	MEMZERO(dest,len)	bzero((dest),(len))

#define	DES_XFORM(buf)							\
		DES_ecb_encrypt(buf, buf, &schedule, 			\
		    mode == MODE_ENCRYPT ? DES_ENCRYPT : DES_DECRYPT);

/*
 * this does an error-checking write
 */
#define	READ(buf, n)	fread(buf, sizeof(char), n, stdin)
#define WRITE(buf,n)						\
		if (fwrite(buf, sizeof(char), n, stdout) != n)	\
			warnx("fwrite error at %d", n);

/*
 * global variables and related macros
 */
#define KEY_DEFAULT		0	/* interpret radix of key from key */
#define KEY_ASCII		1	/* key is in ASCII characters */
int keybase = KEY_DEFAULT;		/* how to interpret the key */

enum { 					/* encrypt, decrypt, authenticate */
	MODE_ENCRYPT, MODE_DECRYPT, MODE_AUTHENTICATE
} mode = MODE_ENCRYPT;

enum {					/* ecb, cbc, cfb, cfba, ofb? */
	ALG_ECB, ALG_CBC, ALG_CFB, ALG_OFB, ALG_CFBA
} alg = ALG_CBC;

DES_cblock ivec;				/* initialization vector */

char bits[] = {				/* used to extract bits from a char */
	'\200', '\100', '\040', '\020', '\010', '\004', '\002', '\001'
};

int inverse;				/* 0 to encrypt, 1 to decrypt */
int macbits = -1;			/* number of bits in authentication */
int fbbits = -1;			/* number of feedback bits */
int pflag;				/* 1 to preserve parity bits */

DES_key_schedule schedule;		/* expanded DES key */

static void ecbenc(void);
static void ecbdec(void);
static void cbcenc(void);
static void cbcdec(void);
static void cfbenc(void);
static void cfbdec(void);
static void cfbaenc(void);
static void cfbadec(void);
static void ofbenc(void);
static void ofbdec(void);

static void cbcauth(void);
static void cfbauth(void);

static void cvtkey(DES_cblock, char *);
static int setbits(char *, int);
static void makekey(DES_cblock *);
static int tobinhex(char, int);

static void usage(void);

int
main(int argc, char *argv[])
{
	extern char *optarg;		/* argument to option if any */
	int i;				/* counter in a for loop */
	char *p;			/* used to obtain the key */
	DES_cblock msgbuf;		/* I/O buffer */
	int kflag;			/* command-line encryption key */

	setproctitle("-");		/* Hide command-line arguments */

	/* initialize the initialization vector */
	MEMZERO(ivec, 8);

	/* process the argument list */
	kflag = 0;
	while ((i = getopt(argc, argv, "abdF:f:k:m:o:pv:")) != EOF)
		switch(i) {
		case 'a':		/* key is ASCII */
			keybase = KEY_ASCII;
			break;
		case 'b':		/* use ECB mode */
			alg = ALG_ECB;
			break;
		case 'd':		/* decrypt */
			mode = MODE_DECRYPT;
			break;
		case 'F':		/* use alternative CFB mode */
			alg = ALG_CFBA;
			if ((fbbits = setbits(optarg, 7)) > 56 || fbbits == 0)
				errx(1, "-F: number must be 1-56 inclusive");
			else if (fbbits == -1)
				errx(1, "-F: number must be a multiple of 7");
			break;
		case 'f':		/* use CFB mode */
			alg = ALG_CFB;
			if ((fbbits = setbits(optarg, 8)) > 64 || fbbits == 0)
				errx(1, "-f: number must be 1-64 inclusive");
			else if (fbbits == -1)
				errx(1, "-f: number must be a multiple of 8");
			break;
		case 'k':		/* encryption key */
			kflag = 1;
			cvtkey(msgbuf, optarg);
			break;
		case 'm':		/* number of bits for MACing */
			mode = MODE_AUTHENTICATE;
			if ((macbits = setbits(optarg, 1)) > 64)
				errx(1, "-m: number must be 0-64 inclusive");
			break;
		case 'o':		/* use OFB mode */
			alg = ALG_OFB;
			if ((fbbits = setbits(optarg, 8)) > 64 || fbbits == 0)
				errx(1, "-o: number must be 1-64 inclusive");
			else if (fbbits == -1)
				errx(1, "-o: number must be a multiple of 8");
			break;
		case 'p':		/* preserve parity bits */
			pflag = 1;
			break;
		case 'v':		/* set initialization vector */
			cvtkey(ivec, optarg);
			break;
		default:		/* error */
			usage();
		}

	if (!kflag) {
		/*
		 * if the key's not ASCII, assume it is
		 */
		keybase = KEY_ASCII;
		/*
		 * get the key
		 */
		p = getpass("Enter key: ");
		/*
		 * copy it, nul-padded, into the key area
		 */
		cvtkey(msgbuf, p);
	}

	makekey(&msgbuf);
	inverse = (alg == ALG_CBC || alg == ALG_ECB) && mode == MODE_DECRYPT;

	switch(alg) {
	case ALG_CBC:
		switch(mode) {
		case MODE_AUTHENTICATE:	/* authenticate using CBC mode */
			cbcauth();
			break;
		case MODE_DECRYPT:	/* decrypt using CBC mode */
			cbcdec();
			break;
		case MODE_ENCRYPT:	/* encrypt using CBC mode */
			cbcenc();
			break;
		}
		break;
	case ALG_CFB:
		switch(mode) {
		case MODE_AUTHENTICATE:	/* authenticate using CFB mode */
			cfbauth();
			break;
		case MODE_DECRYPT:	/* decrypt using CFB mode */
			cfbdec();
			break;
		case MODE_ENCRYPT:	/* encrypt using CFB mode */
			cfbenc();
			break;
		}
		break;
	case ALG_CFBA:
		switch(mode) {
		case MODE_AUTHENTICATE:	/* authenticate using CFBA mode */
			errx(1, "can't authenticate with CFBA mode");
			break;
		case MODE_DECRYPT:	/* decrypt using CFBA mode */
			cfbadec();
			break;
		case MODE_ENCRYPT:	/* encrypt using CFBA mode */
			cfbaenc();
			break;
		}
		break;
	case ALG_ECB:
		switch(mode) {
		case MODE_AUTHENTICATE:	/* authenticate using ECB mode */
			errx(1, "can't authenticate with ECB mode");
			break;
		case MODE_DECRYPT:	/* decrypt using ECB mode */
			ecbdec();
			break;
		case MODE_ENCRYPT:	/* encrypt using ECB mode */
			ecbenc();
			break;
		}
		break;
	case ALG_OFB:
		switch(mode) {
		case MODE_AUTHENTICATE:	/* authenticate using OFB mode */
			errx(1, "can't authenticate with OFB mode");
			break;
		case MODE_DECRYPT:	/* decrypt using OFB mode */
			ofbdec();
			break;
		case MODE_ENCRYPT:	/* encrypt using OFB mode */
			ofbenc();
			break;
		}
		break;
	}
	return (0);
}

/*
 * map a hex character to an integer
 */
static int
tobinhex(char c, int radix)
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
static void
cvtkey(DES_cblock obuf, char *ibuf)
{
	int i, j;			/* counter in a for loop */
	int nbuf[64];			/* used for hex/key translation */

	/*
	 * just switch on the key base
	 */
	switch(keybase) {
	case KEY_ASCII:			/* ascii to integer */
		(void)strncpy(obuf, ibuf, 8);
		return;
	case KEY_DEFAULT:		/* tell from context */
		/*
		 * leading '0x' or '0X' == hex key
		 */
		if (ibuf[0] == '0' && (ibuf[1] == 'x' || ibuf[1] == 'X')) {
			ibuf = &ibuf[2];
			/*
			 * now translate it, bombing on any illegal hex digit
			 */
			for (i = 0; ibuf[i] && i < 16; i++)
				if ((nbuf[i] = tobinhex(ibuf[i], 16)) == -1)
					warnx("bad hex digit in key");
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
				if ((nbuf[i] = tobinhex(ibuf[i], 2)) == -1)
					warnx("bad binary digit in key");
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
}

/*
 * convert an ASCII string into a decimal number:
 * 1. must be between 0 and 64 inclusive
 * 2. must be a valid decimal number
 * 3. must be a multiple of mult
 */
static int
setbits(char *s, int mult)
{
	char *p;			/* pointer in a for loop */
	int n = 0;			/* the integer collected */

	/*
	 * skip white space
	 */
	while (isspace(*s))
		s++;
	/*
	 * get the integer
	 */
	for (p = s; *p; p++) {
		if (isdigit(*p))
			n = n * 10 + *p - '0';
		else {
			warnx("bad decimal digit in MAC length");
		}
	}
	/*
	 * be sure it's a multiple of mult
	 */
	return((n % mult != 0) ? -1 : n);
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
static void
makekey(DES_cblock *buf)
{
	int i, j;				/* counter in a for loop */
	int par;				/* parity counter */

	/*
	 * if the parity is not preserved, flip it
	 */
	if (!pflag) {
		for (i = 0; i < 8; i++) {
			par = 0;
			for (j = 1; j < 8; j++)
				if ((bits[j] & (*buf)[i]) != 0)
					par++;
			if ((par & 0x01) == 0x01)
				(*buf)[i] &= 0x7f;
			else
				(*buf)[i] = ((*buf)[i] & 0x7f) | 0x80;
		}
	}

	DES_set_odd_parity(buf);
	DES_set_key(buf, &schedule);
}

/*
 * This encrypts using the Electronic Code Book mode of DES
 */
static void
ecbenc(void)
{
	int n;				/* number of bytes actually read */
	int bn;				/* block number */
	DES_cblock msgbuf;		/* I/O buffer */

	for (bn = 0; (n = READ(msgbuf,  8)) == 8; bn++) {
		/*
		 * do the transformation
		 */
		DES_XFORM(&msgbuf);
		WRITE(&msgbuf, 8);
	}
	/*
	 * at EOF or last block -- in either case, the last byte contains
	 * the character representation of the number of bytes in it
	 */
	bn++;
	MEMZERO(&msgbuf[n], 8 - n);
	msgbuf[7] = n;
	DES_XFORM(&msgbuf);
	WRITE(&msgbuf, 8);

}

/*
 * This decrypts using the Electronic Code Book mode of DES
 */
static void
ecbdec(void)
{
	int n;			/* number of bytes actually read */
	int c;			/* used to test for EOF */
	int bn;			/* block number */
	DES_cblock msgbuf;		/* I/O buffer */

	for (bn = 1; (n = READ(msgbuf, 8)) == 8; bn++) {
		/*
		 * do the transformation
		 */
		DES_XFORM(&msgbuf);
		/*
		 * if the last one, handle it specially
		 */
		if ((c = getchar()) == EOF) {
			n = msgbuf[7];
			if (n < 0 || n > 7)
				warnx("decryption failed (block corrupt) at %d",
				    bn);
		}
		else
			(void)ungetc(c, stdin);
		WRITE(msgbuf, n);
	}
	if (n > 0)
		warnx("decryption failed (incomplete block) at %d", bn);
}

/*
 * This encrypts using the Cipher Block Chaining mode of DES
 */
static void
cbcenc(void)
{
	int n;			/* number of bytes actually read */
	int bn;			/* block number */
	DES_cblock msgbuf;	/* I/O buffer */

	/*
	 * do the transformation
	 */
	for (bn = 1; (n = READ(msgbuf, 8)) == 8; bn++) {
		for (n = 0; n < 8; n++)
			msgbuf[n] ^= ivec[n];
		DES_XFORM(&msgbuf);
		MEMCPY(ivec, msgbuf, 8);
		WRITE(msgbuf, 8);
	}
	/*
	 * at EOF or last block -- in either case, the last byte contains
	 * the character representation of the number of bytes in it
	 */
	bn++;
	MEMZERO(&msgbuf[n], 8 - n);
	msgbuf[7] = n;
	for (n = 0; n < 8; n++)
		msgbuf[n] ^= ivec[n];
	DES_XFORM(&msgbuf);
	WRITE(msgbuf, 8);

}

/*
 * This decrypts using the Cipher Block Chaining mode of DES
 */
static void
cbcdec(void)
{
	int n;			/* number of bytes actually read */
	DES_cblock msgbuf;	/* I/O buffer */
	DES_cblock ibuf;	/* temp buffer for initialization vector */
	int c;			/* used to test for EOF */
	int bn;			/* block number */

	for (bn = 0; (n = READ(msgbuf, 8)) == 8; bn++) {
		/*
		 * do the transformation
		 */
		MEMCPY(ibuf, msgbuf, 8);
		DES_XFORM(&msgbuf);
		for (c = 0; c < 8; c++)
			msgbuf[c] ^= ivec[c];
		MEMCPY(ivec, ibuf, 8);
		/*
		 * if the last one, handle it specially
		 */
		if ((c = getchar()) == EOF) {
			n = msgbuf[7];
			if (n < 0 || n > 7)
				warnx("decryption failed (block corrupt) at %d",
				    bn);
		}
		else
			(void)ungetc(c, stdin);
		WRITE(msgbuf, n);
	}
	if (n > 0)
		warnx("decryption failed (incomplete block) at %d", bn);
}

/*
 * This authenticates using the Cipher Block Chaining mode of DES
 */
static void
cbcauth(void)
{
	int n, j;		/* number of bytes actually read */
	DES_cblock msgbuf;		/* I/O buffer */
	DES_cblock encbuf;		/* encryption buffer */

	/*
	 * do the transformation
	 * note we DISCARD the encrypted block;
	 * we only care about the last one
	 */
	while ((n = READ(msgbuf, 8)) == 8) {
		for (n = 0; n < 8; n++)
			encbuf[n] = msgbuf[n] ^ ivec[n];
		DES_XFORM(&encbuf);
		MEMCPY(ivec, encbuf, 8);
	}
	/*
	 * now compute the last one, right padding with '\0' if need be
	 */
	if (n > 0) {
		MEMZERO(&msgbuf[n], 8 - n);
		for (n = 0; n < 8; n++)
			encbuf[n] = msgbuf[n] ^ ivec[n];
		DES_XFORM(&encbuf);
	}
	/*
	 * drop the bits
	 * we write chars until fewer than 7 bits,
	 * and then pad the last one with 0 bits
	 */
	for (n = 0; macbits > 7; n++, macbits -= 8)
		(void)putchar(encbuf[n]);
	if (macbits > 0) {
		msgbuf[0] = 0x00;
		for (j = 0; j < macbits; j++)
			msgbuf[0] |= encbuf[n] & bits[j];
		(void)putchar(msgbuf[0]);
	}
}

/*
 * This encrypts using the Cipher FeedBack mode of DES
 */
static void
cfbenc(void)
{
	int n;			/* number of bytes actually read */
	int nbytes;		/* number of bytes to read */
	int bn;			/* block number */
	char ibuf[8];		/* input buffer */
	DES_cblock msgbuf;		/* encryption buffer */

	/*
	 * do things in bytes, not bits
	 */
	nbytes = fbbits / 8;
	/*
	 * do the transformation
	 */
	for (bn = 1; (n = READ(ibuf, nbytes)) == nbytes; bn++) {
		MEMCPY(msgbuf, ivec, 8);
		DES_XFORM(&msgbuf);
		for (n = 0; n < 8 - nbytes; n++)
			ivec[n] = ivec[n+nbytes];
		for (n = 0; n < nbytes; n++)
			ivec[8 - nbytes + n] = ibuf[n] ^ msgbuf[n];
		WRITE(&ivec[8 - nbytes], nbytes);
	}
	/*
	 * at EOF or last block -- in either case, the last byte contains
	 * the character representation of the number of bytes in it
	 */
	bn++;
	MEMZERO(&ibuf[n], nbytes - n);
	ibuf[nbytes - 1] = n;
	MEMCPY(msgbuf, ivec, 8);
	DES_XFORM(&msgbuf);
	for (n = 0; n < nbytes; n++)
		ibuf[n] ^= msgbuf[n];
	WRITE(ibuf, nbytes);
}

/*
 * This decrypts using the Cipher Block Chaining mode of DES
 */
static void
cfbdec(void)
{
	int n;			/* number of bytes actually read */
	int c;			/* used to test for EOF */
	int nbytes;		/* number of bytes to read */
	int bn;			/* block number */
	char ibuf[8];		/* input buffer */
	char obuf[8];		/* output buffer */
	DES_cblock msgbuf;		/* encryption buffer */

	/*
	 * do things in bytes, not bits
	 */
	nbytes = fbbits / 8;
	/*
	 * do the transformation
	 */
	for (bn = 1; (n = READ(ibuf, nbytes)) == nbytes; bn++) {
		MEMCPY(msgbuf, ivec, 8);
		DES_XFORM(&msgbuf);
		for (c = 0; c < 8 - nbytes; c++)
			ivec[c] = ivec[c + nbytes];
		for (c = 0; c < nbytes; c++) {
			ivec[8 - nbytes + c] = ibuf[c];
			obuf[c] = ibuf[c] ^ msgbuf[c];
		}
		/*
		 * if the last one, handle it specially
		 */
		if ((c = getchar()) == EOF) {
			n = obuf[nbytes-1];
			if (n < 0 || n > nbytes-1)
				warnx("decryption failed (block corrupt) at %d",
				    bn);
		}
		else
			(void)ungetc(c, stdin);
		WRITE(obuf, n);
	}
	if (n > 0)
		warnx("decryption failed (incomplete block) at %d", bn);
}

/*
 * This encrypts using the alternative Cipher FeedBack mode of DES
 */
static void
cfbaenc(void)
{
	int n;			/* number of bytes actually read */
	int nbytes;		/* number of bytes to read */
	int bn;			/* block number */
	char ibuf[8];		/* input buffer */
	char obuf[8];		/* output buffer */
	DES_cblock msgbuf;		/* encryption buffer */

	/*
	 * do things in bytes, not bits
	 */
	nbytes = fbbits / 7;
	/*
	 * do the transformation
	 */
	for (bn = 1; (n = READ(ibuf, nbytes)) == nbytes; bn++) {
		MEMCPY(msgbuf, ivec, 8);
		DES_XFORM(&msgbuf);
		for (n = 0; n < 8 - nbytes; n++)
			ivec[n] = ivec[n + nbytes];
		for (n = 0; n < nbytes; n++)
			ivec[8 - nbytes + n] = (ibuf[n] ^ msgbuf[n]) | 0x80;
		for (n = 0; n < nbytes; n++)
			obuf[n] = ivec[8 - nbytes + n] & 0x7f;
		WRITE(obuf, nbytes);
	}
	/*
	 * at EOF or last block -- in either case, the last byte contains
	 * the character representation of the number of bytes in it
	 */
	bn++;
	MEMZERO(&ibuf[n], nbytes - n);
	ibuf[nbytes - 1] = ('0' + n)|0200;
	MEMCPY(msgbuf, ivec, 8);
	DES_XFORM(&msgbuf);
	for (n = 0; n < nbytes; n++)
		ibuf[n] ^= msgbuf[n];
	WRITE(ibuf, nbytes);
}

/*
 * This decrypts using the alternative Cipher Block Chaining mode of DES
 */
static void
cfbadec(void)
{
	int n;			/* number of bytes actually read */
	int c;			/* used to test for EOF */
	int nbytes;		/* number of bytes to read */
	int bn;			/* block number */
	char ibuf[8];		/* input buffer */
	char obuf[8];		/* output buffer */
	DES_cblock msgbuf;		/* encryption buffer */

	/*
	 * do things in bytes, not bits
	 */
	nbytes = fbbits / 7;
	/*
	 * do the transformation
	 */
	for (bn = 1; (n = READ(ibuf, nbytes)) == nbytes; bn++) {
		MEMCPY(msgbuf, ivec, 8);
		DES_XFORM(&msgbuf);
		for (c = 0; c < 8 - nbytes; c++)
			ivec[c] = ivec[c + nbytes];
		for (c = 0; c < nbytes; c++) {
			ivec[8 - nbytes + c] = ibuf[c] | 0x80;
			obuf[c] = (ibuf[c] ^ msgbuf[c]) & 0x7f;
		}
		/*
		 * if the last one, handle it specially
		 */
		if ((c = getchar()) == EOF) {
			if ((n = (obuf[nbytes-1] - '0')) < 0
						|| n > nbytes-1)
				warnx("decryption failed (block corrupt) at %d",
				    bn);
		}
		else
			(void)ungetc(c, stdin);
		WRITE(obuf, n);
	}
	if (n > 0)
		warnx("decryption failed (incomplete block) at %d", bn);
}


/*
 * This encrypts using the Output FeedBack mode of DES
 */
static void
ofbenc(void)
{
	int n;			/* number of bytes actually read */
	int c;			/* used to test for EOF */
	int nbytes;		/* number of bytes to read */
	int bn;			/* block number */
	char ibuf[8];		/* input buffer */
	char obuf[8];		/* output buffer */
	DES_cblock msgbuf;		/* encryption buffer */

	/*
	 * do things in bytes, not bits
	 */
	nbytes = fbbits / 8;
	/*
	 * do the transformation
	 */
	for (bn = 1; (n = READ(ibuf, nbytes)) == nbytes; bn++) {
		MEMCPY(msgbuf, ivec, 8);
		DES_XFORM(&msgbuf);
		for (n = 0; n < 8 - nbytes; n++)
			ivec[n] = ivec[n + nbytes];
		for (n = 0; n < nbytes; n++) {
			ivec[8 - nbytes + n] = msgbuf[n];
			obuf[n] = ibuf[n] ^ msgbuf[n];
		}
		WRITE(obuf, nbytes);
	}
	/*
	 * at EOF or last block -- in either case, the last byte contains
	 * the character representation of the number of bytes in it
	 */
	bn++;
	MEMZERO(&ibuf[n], nbytes - n);
	ibuf[nbytes - 1] = n;
	MEMCPY(msgbuf, ivec, 8);
	DES_XFORM(&msgbuf);
	for (c = 0; c < nbytes; c++)
		ibuf[c] ^= msgbuf[c];
	WRITE(ibuf, nbytes);
}

/*
 * This decrypts using the Output Block Chaining mode of DES
 */
static void
ofbdec(void)
{
	int n;			/* number of bytes actually read */
	int c;			/* used to test for EOF */
	int nbytes;		/* number of bytes to read */
	int bn;			/* block number */
	char ibuf[8];		/* input buffer */
	char obuf[8];		/* output buffer */
	DES_cblock msgbuf;		/* encryption buffer */

	/*
	 * do things in bytes, not bits
	 */
	nbytes = fbbits / 8;
	/*
	 * do the transformation
	 */
	for (bn = 1; (n = READ(ibuf, nbytes)) == nbytes; bn++) {
		MEMCPY(msgbuf, ivec, 8);
		DES_XFORM(&msgbuf);
		for (c = 0; c < 8 - nbytes; c++)
			ivec[c] = ivec[c + nbytes];
		for (c = 0; c < nbytes; c++) {
			ivec[8 - nbytes + c] = msgbuf[c];
			obuf[c] = ibuf[c] ^ msgbuf[c];
		}
		/*
		 * if the last one, handle it specially
		 */
		if ((c = getchar()) == EOF) {
			n = obuf[nbytes-1];
			if (n < 0 || n > nbytes-1)
				warnx("decryption failed (block corrupt) at %d",
				    bn);
		}
		else
			(void)ungetc(c, stdin);
		/*
		 * dump it
		 */
		WRITE(obuf, n);
	}
	if (n > 0)
		warnx("decryption failed (incomplete block) at %d", bn);
}

/*
 * This authenticates using the Cipher FeedBack mode of DES
 */
static void
cfbauth(void)
{
	int n, j;		/* number of bytes actually read */
	int nbytes;		/* number of bytes to read */
	char ibuf[8];		/* input buffer */
	DES_cblock msgbuf;	/* encryption buffer */

	/*
	 * do things in bytes, not bits
	 */
	nbytes = fbbits / 8;
	/*
	 * do the transformation
	 */
	while ((n = READ(ibuf, nbytes)) == nbytes) {
		MEMCPY(msgbuf, ivec, 8);
		DES_XFORM(&msgbuf);
		for (n = 0; n < 8 - nbytes; n++)
			ivec[n] = ivec[n + nbytes];
		for (n = 0; n < nbytes; n++)
			ivec[8 - nbytes + n] = ibuf[n] ^ msgbuf[n];
	}
	/*
	 * at EOF or last block -- in either case, the last byte contains
	 * the character representation of the number of bytes in it
	 */
	MEMZERO(&ibuf[n], nbytes - n);
	ibuf[nbytes - 1] = '0' + n;
	MEMCPY(msgbuf, ivec, 8);
	DES_XFORM(&msgbuf);
	for (n = 0; n < nbytes; n++)
		ibuf[n] ^= msgbuf[n];
	/*
	 * drop the bits
	 * we write chars until fewer than 7 bits,
	 * and then pad the last one with 0 bits
	 */
	for (n = 0; macbits > 7; n++, macbits -= 8)
		(void)putchar(msgbuf[n]);
	if (macbits > 0) {
		msgbuf[0] = 0x00;
		for (j = 0; j < macbits; j++)
			msgbuf[0] |= msgbuf[n] & bits[j];
		(void)putchar(msgbuf[0]);
	}
}

/*
 * message about usage
 */
static void
usage(void)
{
	(void)fprintf(stderr, "%s\n",
"usage: bdes [-abdp] [-F N] [-f N] [-k key] [-m N] [-o N] [-v vector]");
	exit(1);
}
