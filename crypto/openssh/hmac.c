/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
RCSID("$OpenBSD: hmac.c,v 1.4 2000/09/07 20:27:51 deraadt Exp $");

#include "xmalloc.h"
#include "ssh.h"
#include "getput.h"

#include <openssl/hmac.h>

unsigned char *
hmac(
    EVP_MD *evp_md,
    unsigned int seqno,
    unsigned char *data, int datalen,
    unsigned char *key, int keylen)
{
	HMAC_CTX c;
	static unsigned char m[EVP_MAX_MD_SIZE];
	unsigned char b[4];

	if (key == NULL)
		fatal("hmac: no key");
	HMAC_Init(&c, key, keylen, evp_md);
	PUT_32BIT(b, seqno);
	HMAC_Update(&c, b, sizeof b);
	HMAC_Update(&c, data, datalen);
	HMAC_Final(&c, m, NULL);
	HMAC_cleanup(&c);
	return(m);
}
