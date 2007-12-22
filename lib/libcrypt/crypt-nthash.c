/*-
 * Copyright (c) 2003 Michael Bretterklieber
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <netinet/in.h>

#include <ctype.h>
#include <err.h>
#include <md4.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "crypt.h"

/*
 * NT HASH = md4(str2unicode(pw))
 */

/* ARGSUSED */
char *
crypt_nthash(const char *pw, const char *salt __unused)
{
	size_t unipwLen;
	int i, j;
	static char hexconvtab[] = "0123456789abcdef";
	static const char *magic = "$3$";
	static char passwd[120];
	u_int16_t unipw[128];
	char final[MD4_SIZE*2 + 1];
	u_char hash[MD4_SIZE];
	const char *s;
	MD4_CTX	ctx;
  
	bzero(unipw, sizeof(unipw)); 
	/* convert to unicode (thanx Archie) */
	unipwLen = 0;
	for (s = pw; unipwLen < sizeof(unipw) / 2 && *s; s++)
		unipw[unipwLen++] = htons(*s << 8);
        
	/* Compute MD4 of Unicode password */
 	MD4Init(&ctx);
	MD4Update(&ctx, (u_char *)unipw, unipwLen*sizeof(u_int16_t));
	MD4Final(hash, &ctx);  
	
	for (i = j = 0; i < MD4_SIZE; i++) {
		final[j++] = hexconvtab[hash[i] >> 4];
		final[j++] = hexconvtab[hash[i] & 15];
	}
	final[j] = '\0';

	strcpy(passwd, magic);
	strcat(passwd, "$");
	strncat(passwd, final, MD4_SIZE*2);

	/* Don't leave anything around in vm they could use. */
	memset(final, 0, sizeof(final));

	return (passwd);
}
