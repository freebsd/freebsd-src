/*-
 * Copyright (c) 1991, 1993
 *      Dave Safford.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 * 
 * $FreeBSD$
 */

/* header for the des routines that we will use */

typedef unsigned char byte, DesData[ 8], IdeaData[16];
#if 0
typedef unsigned long word, DesKeys[32];
#else
#define DesKeys des_key_schedule
#endif

#define DES_DECRYPT 0
#define DES_ENCRYPT 1

#if 0
extern void des_fixup_key_parity();	/* (DesData *key) */
extern int des_key_sched(); 	/* (DesData *key, DesKeys *m) */
extern int des_ecb_encrypt();	/* (DesData *src, *dst, DesKeys *m, int mode) */
extern int des_cbc_encrypt();   /* (char *src, *dst, int length,
				    DesKeys *m, DesData *init, int mode) */
#endif

/* public key routines */
/* functions:
	genkeys(char *public, char *secret)
	common_key(char *secret, char *public, desData *deskey)
      where
	char public[HEXKEYBYTES + 1];
	char secret[HEXKEYBYTES + 1];
 */

#define HEXMODULUS "d4a0ba0250b6fd2ec626e7efd637df76c716e22d0944b88b"
#define HEXKEYBYTES 48
#define KEYSIZE 192
#define KEYBYTES 24
#define PROOT 3

extern void genkeys(char *public, char *secret);
extern void common_key(char *secret, char *public, IdeaData *common,
  DesData *deskey);
extern void pk_encode(char *in, char *out, DesData *deskey);
extern void pk_decode(char *in, char *out, DesData *deskey);

