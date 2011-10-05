/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hmac.h>
#include <evp.h>
#include <roken.h>

int
main(int argc, char **argv)
{
    unsigned char buf[4] = { 0, 0, 0, 0 };
    char hmackey[] = "hello-world";
    size_t hmackey_size = sizeof(hmackey);
    unsigned int hmaclen;
    unsigned char hmac[EVP_MAX_MD_SIZE];
    HMAC_CTX c;

    char answer[20] = "\x2c\xfa\x32\xb7\x2b\x8a\xf6\xdf\xcf\xda"
	              "\x6f\xd1\x52\x4d\x54\x58\x73\x0f\xf3\x24";

    HMAC_CTX_init(&c);
    HMAC_Init_ex(&c, hmackey, hmackey_size, EVP_sha1(), NULL);
    HMAC_Update(&c, buf, sizeof(buf));
    HMAC_Final(&c, hmac, &hmaclen);
    HMAC_CTX_cleanup(&c);

    if (hmaclen != 20) {
	printf("hmaclen = %d\n", (int)hmaclen);
	return 1;
    }

    if (ct_memcmp(hmac, answer, hmaclen) != 0) {
	printf("wrong answer\n");
	return 1;
    }

    return 0;
}
