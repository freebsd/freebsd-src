/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include "config.h"
RCSID("$Id: otp_md.c,v 1.15 2001/08/22 20:30:32 assar Exp $");
#endif
#include "otp_locl.h"

#include "otp_md.h"
#ifdef HAVE_OPENSSL
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#else
#include <md4.h>
#include <md5.h>
#include <sha.h>
#endif

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

static int
otp_md_init (OtpKey key,
	     const char *pwd,
	     const char *seed,
	     void (*init)(void *),
	     void (*update)(void *, const void *, size_t),
	     void (*final)(void *, void *),
	     void *arg,
	     unsigned char *res,
	     size_t ressz)
{
  char *p;
  int len;

  len = strlen(pwd) + strlen(seed);
  p = malloc (len + 1);
  if (p == NULL)
    return -1;
  strcpy (p, seed);
  strlwr (p);
  strcat (p, pwd);
  (*init)(arg);
  (*update)(arg, p, len);
  (*final)(res, arg);
  free (p);
  compressmd (key, res, ressz);
  return 0;
}

static int
otp_md_next (OtpKey key,
	     void (*init)(void *),
	     void (*update)(void *, const void *, size_t),
	     void (*final)(void *, void *),
	     void *arg,
	     unsigned char *res,
	     size_t ressz)
{
  (*init)(arg);
  (*update)(arg, key, OTPKEYSIZE);
  (*final)(res, arg);
  compressmd (key, res, ressz);
  return 0;
}

static int
otp_md_hash (const char *data,
	     size_t len,
	     void (*init)(void *),
	     void (*update)(void *, const void *, size_t),
	     void (*final)(void *, void *),
	     void *arg,
	     unsigned char *res,
	     size_t ressz)
{
  (*init)(arg);
  (*update)(arg, data, len);
  (*final)(res, arg);
  return 0;
}

int
otp_md4_init (OtpKey key, const char *pwd, const char *seed)
{
  unsigned char res[16];
  MD4_CTX md4;

  return otp_md_init (key, pwd, seed,
		      (void (*)(void *))MD4_Init,
		      (void (*)(void *, const void *, size_t))MD4_Update, 
		      (void (*)(void *, void *))MD4_Final,
		      &md4, res, sizeof(res));
}

int
otp_md4_hash (const char *data,
	      size_t len,
	      unsigned char *res)
{
  MD4_CTX md4;

  return otp_md_hash (data, len,
		      (void (*)(void *))MD4_Init,
		      (void (*)(void *, const void *, size_t))MD4_Update, 
		      (void (*)(void *, void *))MD4_Final,
		      &md4, res, 16);
}

int
otp_md4_next (OtpKey key)
{
  unsigned char res[16];
  MD4_CTX md4;

  return otp_md_next (key, 
		      (void (*)(void *))MD4_Init, 
		      (void (*)(void *, const void *, size_t))MD4_Update, 
		      (void (*)(void *, void *))MD4_Final,
		      &md4, res, sizeof(res));
}


int
otp_md5_init (OtpKey key, const char *pwd, const char *seed)
{
  unsigned char res[16];
  MD5_CTX md5;

  return otp_md_init (key, pwd, seed, 
		      (void (*)(void *))MD5_Init, 
		      (void (*)(void *, const void *, size_t))MD5_Update, 
		      (void (*)(void *, void *))MD5_Final,
		      &md5, res, sizeof(res));
}

int
otp_md5_hash (const char *data,
	      size_t len,
	      unsigned char *res)
{
  MD5_CTX md5;

  return otp_md_hash (data, len,
		      (void (*)(void *))MD5_Init,
		      (void (*)(void *, const void *, size_t))MD5_Update, 
		      (void (*)(void *, void *))MD5_Final,
		      &md5, res, 16);
}

int
otp_md5_next (OtpKey key)
{
  unsigned char res[16];
  MD5_CTX md5;

  return otp_md_next (key, 
		      (void (*)(void *))MD5_Init, 
		      (void (*)(void *, const void *, size_t))MD5_Update, 
		      (void (*)(void *, void *))MD5_Final,
		      &md5, res, sizeof(res));
}

/* 
 * For histerical reasons, in the OTP definition it's said that the
 * result from SHA must be stored in little-endian order.  See
 * draft-ietf-otp-01.txt.
 */

static void
SHA1_Final_little_endian (void *res, SHA_CTX *m)
{
  unsigned char tmp[20];
  unsigned char *p = res;
  int j;

  SHA1_Final (tmp, m);
  for (j = 0; j < 20; j += 4) {
    p[j]   = tmp[j+3];
    p[j+1] = tmp[j+2];
    p[j+2] = tmp[j+1];
    p[j+3] = tmp[j];
  }
}

int
otp_sha_init (OtpKey key, const char *pwd, const char *seed)
{
  unsigned char res[20];
  SHA_CTX sha1;

  return otp_md_init (key, pwd, seed, 
		      (void (*)(void *))SHA1_Init, 
		      (void (*)(void *, const void *, size_t))SHA1_Update, 
		      (void (*)(void *, void *))SHA1_Final_little_endian,
		      &sha1, res, sizeof(res));
}

int
otp_sha_hash (const char *data,
	      size_t len,
	      unsigned char *res)
{
  SHA_CTX sha1;

  return otp_md_hash (data, len,
		      (void (*)(void *))SHA1_Init,
		      (void (*)(void *, const void *, size_t))SHA1_Update, 
		      (void (*)(void *, void *))SHA1_Final_little_endian,
		      &sha1, res, 20);
}

int
otp_sha_next (OtpKey key)
{
  unsigned char res[20];
  SHA_CTX sha1;

  return otp_md_next (key, 
		      (void (*)(void *))SHA1_Init,
		      (void (*)(void *, const void *, size_t))SHA1_Update,
		      (void (*)(void *, void *))SHA1_Final_little_endian,
		      &sha1, res, sizeof(res));
}
