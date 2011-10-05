/*
 * Copyright (c) 1995 - 2003 Kungliga Tekniska Högskolan
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

#define HC_DEPRECATED_CRYPTO

#include "config.h"

#include "otp_locl.h"

#include "otp_md.h"
#include "crypto-headers.h"

/*
 * Compress len bytes from md into key
 */

static void
compressmd (OtpKey key, unsigned char *md, size_t len)
{
    u_char *p = key;

    memset (p, 0, OTPKEYSIZE);
    while(len) {
	*p++ ^= *md++;
	*p++ ^= *md++;
	*p++ ^= *md++;
	*p++ ^= *md++;
	len -= 4;
	if (p == key + OTPKEYSIZE)
	    p = key;
    }
}

/*
 * For histerical reasons, in the OTP definition it's said that
 * the result from SHA must be stored in little-endian order.  See
 * draft-ietf-otp-01.txt.
 */

static void
little_endian(unsigned char *res, size_t len)
{
    unsigned char t;
    size_t i;

    for (i = 0; i < len; i += 4) {
	t = res[i + 0]; res[i + 0] = res[i + 3]; res[i + 3] = t;
	t = res[i + 1]; res[i + 1] = res[i + 2]; res[i + 2] = t;
    }
}

static int
otp_md_init (OtpKey key,
	     const char *pwd,
	     const char *seed,
	     const EVP_MD *md,
	     int le,
	     unsigned char *res,
	     size_t ressz)
{
    EVP_MD_CTX *ctx;
    char *p;
    int len;

    ctx = EVP_MD_CTX_create();

    len = strlen(pwd) + strlen(seed);
    p = malloc (len + 1);
    if (p == NULL)
	return -1;
    strlcpy (p, seed, len + 1);
    strlwr (p);
    strlcat (p, pwd, len + 1);

    EVP_DigestInit_ex(ctx, md, NULL);
    EVP_DigestUpdate(ctx, p, len);
    EVP_DigestFinal_ex(ctx, res, NULL);

    EVP_MD_CTX_destroy(ctx);

    if (le)
    	little_endian(res, ressz);

    free (p);
    compressmd (key, res, ressz);
    return 0;
}

static int
otp_md_next (OtpKey key,
	     const EVP_MD *md,
	     int le,
	     unsigned char *res,
	     size_t ressz)
{
    EVP_MD_CTX *ctx;

    ctx = EVP_MD_CTX_create();

    EVP_DigestInit_ex(ctx, md, NULL);
    EVP_DigestUpdate(ctx, key, OTPKEYSIZE);
    EVP_DigestFinal_ex(ctx, res, NULL);

    EVP_MD_CTX_destroy(ctx);

    if (le)
	little_endian(res, ressz);

    compressmd (key, res, ressz);
    return 0;
}

static int
otp_md_hash (const char *data,
	     size_t len,
	     const EVP_MD *md,
	     int le,
	     unsigned char *res,
	     size_t ressz)
{
    EVP_MD_CTX *ctx;
    ctx = EVP_MD_CTX_create();

    EVP_DigestInit_ex(ctx, md, NULL);
    EVP_DigestUpdate(ctx, data, len);
    EVP_DigestFinal_ex(ctx, res, NULL);

    EVP_MD_CTX_destroy(ctx);

    if (le)
	little_endian(res, ressz);

    return 0;
}

int
otp_md4_init (OtpKey key, const char *pwd, const char *seed)
{
  unsigned char res[16];
  return otp_md_init (key, pwd, seed, EVP_md4(), 0, res, sizeof(res));
}

int
otp_md4_hash (const char *data,
	      size_t len,
	      unsigned char *res)
{
  return otp_md_hash (data, len, EVP_md4(), 0, res, 16);
}

int
otp_md4_next (OtpKey key)
{
  unsigned char res[16];
  return otp_md_next (key, EVP_md4(), 0, res, sizeof(res));
}


int
otp_md5_init (OtpKey key, const char *pwd, const char *seed)
{
  unsigned char res[16];
  return otp_md_init (key, pwd, seed, EVP_md5(), 0, res, sizeof(res));
}

int
otp_md5_hash (const char *data,
	      size_t len,
	      unsigned char *res)
{
  return otp_md_hash (data, len, EVP_md5(), 0, res, 16);
}

int
otp_md5_next (OtpKey key)
{
  unsigned char res[16];
  return otp_md_next (key, EVP_md5(), 0, res, sizeof(res));
}

int
otp_sha_init (OtpKey key, const char *pwd, const char *seed)
{
  unsigned char res[20];
  return otp_md_init (key, pwd, seed, EVP_sha1(), 1, res, sizeof(res));
}

int
otp_sha_hash (const char *data,
	      size_t len,
	      unsigned char *res)
{
  return otp_md_hash (data, len, EVP_sha1(), 1, res, 20);
}

int
otp_sha_next (OtpKey key)
{
  unsigned char res[20];
  return otp_md_next (key, EVP_sha1(), 1, res, sizeof(res));
}
