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
static char rcsid[] = "$FreeBSD: src/lib/libc/gen/crypt.c,v 1.5 1999/08/28 05:03:17 peter Exp $";
#endif /* LIBC_SCCS and not lint */

#include <unistd.h>
#include <stdio.h>
#include <string.h>

/*
 * UNIX password, and DES, encryption.
 *
 * since this is non-exportable, this is just a dummy.  if you want real
 * encryption, make sure you've got libcrypt.a around.
 */

__warn_references(des_setkey,
	"WARNING!  des_setkey(3) not present in the system!");

int
des_setkey(key)
	register const char *key;
{
	fprintf(stderr, "WARNING!  des_setkey(3) not present in the system!\n");
	return (0);
}

__warn_references(des_cipher,
	"WARNING!  des_cipher(3) not present in the system!");

int
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

__warn_references(setkey,
	"WARNING!  setkey(3) not present in the system!");

int
setkey(key)
	register const char *key;
{
	fprintf(stderr, "WARNING!  setkey(3) not present in the system!\n");
	return (0);
}

__warn_references(encrypt,
	"WARNING!  encrypt(3) not present in the system!");

int
encrypt(block, flag)
	register char  *block;
	int             flag;
{
	fprintf(stderr, "WARNING!  encrypt(3) not present in the system!\n");
	return (0);
}
