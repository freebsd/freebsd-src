/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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

/* $Id$ */

#ifndef _OTP_H
#define _OTP_H

#include <stdlib.h>
#include <time.h>

enum {OTPKEYSIZE = 8};

typedef unsigned char OtpKey[OTPKEYSIZE];

#define OTP_MIN_PASSPHRASE 10
#define OTP_MAX_PASSPHRASE 63

#define OTP_USER_TIMEOUT   120
#define OTP_DB_TIMEOUT      60

#define OTP_HEXPREFIX "hex:"
#define OTP_WORDPREFIX "word:"

typedef enum { OTP_ALG_MD4, OTP_ALG_MD5, OTP_ALG_SHA } OtpAlgID;

#define OTP_ALG_DEFAULT "md5"

typedef struct {
  OtpAlgID id;
  char *name;
  int hashsize;
  int (*hash)(const char *, size_t, unsigned char *);
  int (*init)(OtpKey, const char *, const char *);
  int (*next)(OtpKey);
} OtpAlgorithm;

typedef struct {
  char *user;
  OtpAlgorithm *alg;
  unsigned n;
  char seed[17];
  OtpKey key;
  int challengep;
  time_t lock_time;
  char *err;
} OtpContext;

OtpAlgorithm *otp_find_alg (char *);
void otp_print_stddict (OtpKey, char *, size_t);
void otp_print_hex (OtpKey, char *, size_t);
void otp_print_stddict_extended (OtpKey, char *, size_t);
void otp_print_hex_extended (OtpKey, char *, size_t);
unsigned otp_checksum (OtpKey);
int otp_parse_hex (OtpKey, const char *);
int otp_parse_stddict (OtpKey, const char *);
int otp_parse_altdict (OtpKey, const char *, OtpAlgorithm *);
int otp_parse (OtpKey, const char *, OtpAlgorithm *);
int otp_challenge (OtpContext *, char *, char *, size_t);
int otp_verify_user (OtpContext *, const char *);
int otp_verify_user_1 (OtpContext *, const char *);
char *otp_error (OtpContext *);

void *otp_db_open (void);
void otp_db_close (void *);
int otp_put (void *, OtpContext *);
int otp_get (void *, OtpContext *);
int otp_simple_get (void *, OtpContext *);
int otp_delete (void *, OtpContext *);

#endif /* _OTP_H */
