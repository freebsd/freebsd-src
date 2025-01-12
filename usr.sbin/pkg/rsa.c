/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012-2014 Baptiste Daroussin <bapt@FreeBSD.org>
 * Copyright (c) 2013 Bryan Drewery <bdrewery@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/types.h>

#include <err.h>
#include <stdbool.h>
#include <string.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "pkg.h"

#include "config.h"
#include "hash.h"

static EVP_PKEY *
load_public_key_file(const char *file)
{
	EVP_PKEY *pkey;
	BIO *bp;
	char errbuf[1024];

	bp = BIO_new_file(file, "r");
	if (!bp)
		errx(EXIT_FAILURE, "Unable to read %s", file);

	if ((pkey = PEM_read_bio_PUBKEY(bp, NULL, NULL, NULL)) == NULL)
		warnx("ici: %s", ERR_error_string(ERR_get_error(), errbuf));

	BIO_free(bp);

	return (pkey);
}

static EVP_PKEY *
load_public_key_buf(const unsigned char *cert, int certlen)
{
	EVP_PKEY *pkey;
	BIO *bp;
	char errbuf[1024];

	bp = BIO_new_mem_buf(__DECONST(void *, cert), certlen);

	if ((pkey = PEM_read_bio_PUBKEY(bp, NULL, NULL, NULL)) == NULL)
		warnx("%s", ERR_error_string(ERR_get_error(), errbuf));

	BIO_free(bp);

	return (pkey);
}

static bool
rsa_verify_data(const struct pkgsign_ctx *ctx __unused,
    const char *data, size_t datasz, const char *sigfile,
    const unsigned char *key, int keylen, unsigned char *sig, int siglen)
{
	EVP_MD_CTX *mdctx;
	EVP_PKEY *pkey;
	char errbuf[1024];
	bool ret;

	pkey = NULL;
	mdctx = NULL;
	ret = false;
	SSL_load_error_strings();

	if (sigfile != NULL) {
		if ((pkey = load_public_key_file(sigfile)) == NULL) {
			warnx("Error reading public key");
			goto cleanup;
		}
	} else {
		if ((pkey = load_public_key_buf(key, keylen)) == NULL) {
			warnx("Error reading public key");
			goto cleanup;
		}
	}

	/* Verify signature of the SHA256(pkg) is valid. */
	if ((mdctx = EVP_MD_CTX_create()) == NULL) {
		warnx("%s", ERR_error_string(ERR_get_error(), errbuf));
		goto error;
	}

	if (EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, pkey) != 1) {
		warnx("%s", ERR_error_string(ERR_get_error(), errbuf));
		goto error;
	}
	if (EVP_DigestVerifyUpdate(mdctx, data, datasz) != 1) {
		warnx("%s", ERR_error_string(ERR_get_error(), errbuf));
		goto error;
	}

	if (EVP_DigestVerifyFinal(mdctx, sig, siglen) != 1) {
		warnx("%s", ERR_error_string(ERR_get_error(), errbuf));
		goto error;
	}

	ret = true;
	printf("done\n");
	goto cleanup;

error:
	printf("failed\n");

cleanup:
	if (pkey)
		EVP_PKEY_free(pkey);
	if (mdctx)
		EVP_MD_CTX_destroy(mdctx);
	ERR_free_strings();

	return (ret);
}

static bool
rsa_verify_cert(const struct pkgsign_ctx *ctx __unused, int fd,
    const char *sigfile, const unsigned char *key, int keylen,
    unsigned char *sig, int siglen)
{
	char *sha256;
	bool ret;

	sha256 = NULL;

	/* Compute SHA256 of the package. */
	if (lseek(fd, 0, 0) == -1) {
		warn("lseek");
		return (false);
	}
	if ((sha256 = sha256_fd(fd)) == NULL) {
		warnx("Error creating SHA256 hash for package");
		return (false);
	}

	ret = rsa_verify_data(ctx, sha256, strlen(sha256), sigfile, key, keylen,
	    sig, siglen);
	free(sha256);

	return (ret);
}

const struct pkgsign_ops pkgsign_rsa = {
	.pkgsign_verify_cert = rsa_verify_cert,
	.pkgsign_verify_data = rsa_verify_data,
};
