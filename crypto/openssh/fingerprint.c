/*
 * Copyright (c) 1999 Markus Friedl.  All rights reserved.
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
 *      This product includes software developed by Markus Friedl.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
RCSID("$Id: fingerprint.c,v 1.4 1999/11/24 16:15:25 markus Exp $");

#include "ssh.h"
#include "xmalloc.h"
#include <ssl/md5.h>

#define FPRINT "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x"

/*
 * Generate key fingerprint in ascii format.
 * Based on ideas and code from Bjoern Groenvall <bg@sics.se>
 */
char *
fingerprint(BIGNUM *e, BIGNUM *n)
{
	static char retval[80];
	MD5_CTX md;
	unsigned char d[16];
	char *buf;
	int nlen, elen;

	nlen = BN_num_bytes(n);
	elen = BN_num_bytes(e);

	buf = xmalloc(nlen + elen);

	BN_bn2bin(n, buf);
	BN_bn2bin(e, buf + nlen);

	MD5_Init(&md);
	MD5_Update(&md, buf, nlen + elen);
	MD5_Final(d, &md);
	snprintf(retval, sizeof(retval), FPRINT,
	    d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7],
	    d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
	memset(buf, 0, nlen + elen);
	xfree(buf);
	return retval;
}
