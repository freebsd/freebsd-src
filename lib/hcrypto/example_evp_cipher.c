/*
 * Copyright (c) 2008 Kungliga Tekniska Högskolan
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

#include <krb5-types.h> /* should really be stdint.h */
#include <hcrypto/evp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <assert.h>

#include "roken.h"

/* key and initial vector */
static char key[16] =
    "\xaa\xbb\x45\xd4\xaa\xbb\x45\xd4"
    "\xaa\xbb\x45\xd4\xaa\xbb\x45\xd4";
static char ivec[16] =
    "\xaa\xbb\x45\xd4\xaa\xbb\x45\xd4"
    "\xaa\xbb\x45\xd4\xaa\xbb\x45\xd4";

static void
usage(int exit_code) __attribute__((noreturn));

static void
usage(int exit_code)
{
    printf("usage: %s in out\n", getprogname());
    exit(exit_code);
}


int
main(int argc, char **argv)
{
    int encryptp = 1;
    const char *ifn = NULL, *ofn = NULL;
    FILE *in, *out;
    void *ibuf, *obuf;
    int ilen, olen;
    size_t block_size = 0;
    const EVP_CIPHER *c = EVP_aes_128_cbc();
    EVP_CIPHER_CTX ctx;
    int ret;

    setprogname(argv[0]);

    if (argc == 2) {
	if (strcmp(argv[1], "--version") == 0) {
	    printf("version");
	    exit(0);
	}
	if (strcmp(argv[1], "--help") == 0)
	    usage(0);
	usage(1);
    } else if (argc == 4) {
	block_size = atoi(argv[1]);
	if (block_size == 0)
	    errx(1, "invalid blocksize %s", argv[1]);
	ifn = argv[2];
	ofn = argv[3];
    } else
	usage(1);

    in = fopen(ifn, "r");
    if (in == NULL)
	errx(1, "failed to open input file");
    out = fopen(ofn, "w+");
    if (out == NULL)
	errx(1, "failed to open output file");

    /* Check that key and ivec are long enough */
    assert(EVP_CIPHER_key_length(c) <= sizeof(key));
    assert(EVP_CIPHER_iv_length(c) <= sizeof(ivec));

    /*
     * Allocate buffer, the output buffer is at least
     * EVP_CIPHER_block_size() longer
     */
    ibuf = malloc(block_size);
    obuf = malloc(block_size + EVP_CIPHER_block_size(c));

    /*
     * Init the memory used for EVP_CIPHER_CTX and set the key and
     * ivec.
     */
    EVP_CIPHER_CTX_init(&ctx);
    EVP_CipherInit_ex(&ctx, c, NULL, key, ivec, encryptp);

    /* read in buffer */
    while ((ilen = fread(ibuf, 1, block_size, in)) > 0) {
	/* encrypto/decrypt */
	ret = EVP_CipherUpdate(&ctx, obuf, &olen, ibuf, ilen);
	if (ret != 1) {
	    EVP_CIPHER_CTX_cleanup(&ctx);
	    errx(1, "EVP_CipherUpdate failed");
	}
	/* write out to output file */
	fwrite(obuf, 1, olen, out);
    }
    /* done reading */
    fclose(in);

    /* clear up any last bytes left in the output buffer */
    ret = EVP_CipherFinal_ex(&ctx, obuf, &olen);
    EVP_CIPHER_CTX_cleanup(&ctx);
    if (ret != 1)
	errx(1, "EVP_CipherFinal_ex failed");

    /* write the last bytes out and close */
    fwrite(obuf, 1, olen, out);
    fclose(out);

    return 0;
}
