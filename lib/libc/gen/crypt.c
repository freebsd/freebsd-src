/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Tom Truscott.
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

#if defined(LIBC_SCCS) && !defined(lint)
/* from static char sccsid[] = "@(#)crypt.c	5.11 (Berkeley) 6/25/91"; */
static char rcsid[] = "$Header: /a/cvs/386BSD/src/lib/libc/gen/crypt.c,v 1.6 1993/08/29 22:03:56 nate Exp $";
#endif /* LIBC_SCCS and not lint */

#include <unistd.h>
#include <stdio.h>

/*
 * UNIX password, and DES, encryption.
 * 
 * since this is non-exportable, this is just a dummy.  if you want real
 * encryption, make sure you've got libcrypt.a around.
 */

#ifndef DES
#define SCRAMBLE		/* Don't leave them in plaintext */
#endif

#ifndef SCRAMBLE
static char     cryptresult[1+4+4+11+1];        /* "encrypted" result */

char *
crypt(key, setting)
	register const char *key;
	register const char *setting;
{
	fprintf(stderr, "WARNING!  crypt(3) not present in the system!\n");
	strncpy(cryptresult, key, sizeof cryptresult);
	cryptresult[sizeof cryptresult - 1] = '\0';
	return (cryptresult);
}

#else

char *
crypt(pw, salt)
	register const char *pw;
	register const char *salt;
{
	static char     password[14];
	long            matrix[128], *m, vector[2];
	char            a, b, *p;
	int             i, value;
	unsigned short  crc;
	unsigned long   t;

	/* Ugly hack, but I'm too lazy to find the real problem - NW */
	bzero(matrix, 128 * sizeof(long));

	if (salt[0]) {
		a = salt[0];
		if (salt[1])
			b = salt[1];
		else
			b = a;
	} else
		a = b = '0';
	password[0] = a;
	password[1] = b;
	if (a > 'Z')
		a -= 6;
	if (a > '9')
		a -= 7;
	if (b > 'Z')
		b -= 6;
	if (b > '9')
		b -= 7;
	a -= '.';
	b -= '.';
	value = (a | (b << 6)) & 07777;

	crc = value;
	value += 1000;
	b = 0;
	p = (char *)pw;
	while (value--) {
		if (crc & 0x8000)
			crc = (crc << 1) ^ 0x1021;
		else
			crc <<= 1;
		if (!b) {
			b = 8;
			if (!(i = *p++)) {
				p = (char *)pw;
				i = *p++;
			}
		}
		if (i & 0x80)
			crc ^= 1;
		i <<= 1;
		b--;
	}

	m = matrix;
	matrix[0] = 0;
	a = 32;
	for (value = 07777; value >= 0; value--) {
		*m <<= 1;
		if (crc & 0x8000) {
			*m |= 1;
			crc = (crc << 1) ^ 0x1021;
		} else
			crc <<= 1;
		if (!b) {
			b = 8;
			if (!(i = *p++)) {
				p = (char *)pw;
				i = *p++;
			}
		}
		if (i & 0x80)
			crc ^= 1;
		i <<= 1;
		b--;
		if (!(a--)) {
			a = 32;
			*++m = 0;
		}
	}

	vector[0] = 0;
	vector[1] = 0;
	p = (char *) vector;
	for (i = 0; i < 7; i++)
		if (pw[i])
			*p++ = pw[i];
		else
			break;

	p = password + 2;
	a = 6;
	m = matrix;
	*p = 0;
	for (i = 077; i >= 0; i--) {
		t = *m++;
		t = t ^ *m++;
		t = t ^ vector[0];
		t = t ^ vector[1];
		b = 0;
		while (t) {
			if (t & 1)
				b = 1 - b;
			t >>= 1;
		}
		a--;
		if (b)
			*p |= 1 << a;
		if (!a) {
			a = 6;
			*++p = 0;
		}
	}

	for (i = 2; i < 13; i++) {
		password[i] += '.';
		if (password[i] > '9')
			password[i] += 7;
		if (password[i] > 'Z')
			password[i] += 6;
	}
	password[13] = 0;

	return password;
}
#endif

des_setkey(key)
	register const char *key;
{
	fprintf(stderr, "WARNING!  des_setkey(3) not present in the system!\n");
	return (0);
}

des_cipher(in, out, salt, num_iter)
	const char     *in;
	char           *out;
	long            salt;
	int             num_iter;
{
	fprintf(stderr, "WARNING!  des_cipher(3) not present in the system!\n");
	bcopy(in, out, 8);
	return (0);
}

setkey(key)
	register const char *key;
{
	fprintf(stderr, "WARNING!  setkey(3) not present in the system!\n");
	return (0);
}

encrypt(block, flag)
	register char  *block;
	int             flag;
{
	fprintf(stderr, "WARNING!  encrypt(3) not present in the system!\n");
	return (0);
}
