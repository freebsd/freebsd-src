/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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
RCSID("$Id: otp_db.c,v 1.17 1999/12/02 16:58:44 joda Exp $");
#endif

#include "otp_locl.h"

#define RETRIES 5

void *
otp_db_open (void)
{
  int lock;
  int i;
  void *ret;

  for(i = 0; i < RETRIES; ++i) {
    struct stat statbuf;

    lock = open (OTP_DB_LOCK, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (lock >= 0) {
      close(lock);
      break;
    }
    if (stat (OTP_DB_LOCK, &statbuf) == 0) {
      if (time(NULL) - statbuf.st_mtime > OTP_DB_TIMEOUT)
	unlink (OTP_DB_LOCK);
      else
	sleep (1);
    }
  }
  if (i == RETRIES)
    return NULL;
  ret = dbm_open (OTP_DB, O_RDWR | O_CREAT, 0600);
  if (ret == NULL)
    unlink (OTP_DB_LOCK);
  return ret;
}

void
otp_db_close (void *dbm)
{
  dbm_close ((DBM *)dbm);
  unlink (OTP_DB_LOCK);
}

/*
 * Remove this entry from the database.
 * return 0 if ok.
 */

int 
otp_delete (void *v, OtpContext *ctx)
{
  DBM *dbm = (DBM *)v;
  datum key;

  key.dsize = strlen(ctx->user);
  key.dptr  = ctx->user;
 
  return dbm_delete(dbm, key);
}

/*
 * Read this entry from the database and lock it if lockp.
 */

static int
otp_get_internal (void *v, OtpContext *ctx, int lockp)
{
  DBM *dbm = (DBM *)v;
  datum dat, key;
  char *p;
  time_t now, then;

  key.dsize = strlen(ctx->user);
  key.dptr  = ctx->user;

  dat = dbm_fetch (dbm, key);
  if (dat.dptr == NULL) {
    ctx->err = "Entry not found";
    return -1;
  }
  p = dat.dptr;

  memcpy (&then, p, sizeof(then));
  ctx->lock_time = then;
  if (lockp) {
    time(&now);
    if (then && now - then < OTP_USER_TIMEOUT) {
      ctx->err = "Entry locked";
      return -1;
    }
    memcpy (p, &now, sizeof(now));
  }
  p += sizeof(now);
  ctx->alg = otp_find_alg (p);
  if (ctx->alg == NULL) {
    ctx->err = "Bad algorithm";
    return -1;
  }
  p += strlen(p) + 1;
  {
    unsigned char *up = (unsigned char *)p;
    ctx->n = (up[0] << 24) | (up[1] << 16) | (up[2] << 8) | up[3];
  }
  p += 4;
  memcpy (ctx->key, p, OTPKEYSIZE);
  p += OTPKEYSIZE;
  strlcpy (ctx->seed, p, sizeof(ctx->seed));
  if (lockp)
    return dbm_store (dbm, key, dat, DBM_REPLACE);
  else
    return 0;
}

/*
 * Get and lock.
 */

int
otp_get (void *v, OtpContext *ctx)
{
  return otp_get_internal (v, ctx, 1);
}

/*
 * Get and don't lock.
 */

int
otp_simple_get (void *v, OtpContext *ctx)
{
  return otp_get_internal (v, ctx, 0);
}

/*
 * Write this entry to the database.
 */

int
otp_put (void *v, OtpContext *ctx)
{
  DBM *dbm = (DBM *)v;
  datum dat, key;
  char buf[1024], *p;
  time_t zero = 0;
  size_t len, rem;

  key.dsize = strlen(ctx->user);
  key.dptr  = ctx->user;

  p = buf;
  rem = sizeof(buf);

  if (rem < sizeof(zero))
      return -1;
  memcpy (p, &zero, sizeof(zero));
  p += sizeof(zero);
  rem -= sizeof(zero);
  len = strlen(ctx->alg->name) + 1;

  if (rem < len)
      return -1;
  strcpy (p, ctx->alg->name);
  p += len;
  rem -= len;

  if (rem < 4)
      return -1;
  {
    unsigned char *up = (unsigned char *)p;
    *up++ = (ctx->n >> 24) & 0xFF;
    *up++ = (ctx->n >> 16) & 0xFF;
    *up++ = (ctx->n >>  8) & 0xFF;
    *up++ = (ctx->n >>  0) & 0xFF;
  }
  p += 4;
  rem -= 4;

  if (rem < OTPKEYSIZE)
      return -1;
  memcpy (p, ctx->key, OTPKEYSIZE);
  p += OTPKEYSIZE;
  rem -= OTPKEYSIZE;

  len = strlen(ctx->seed) + 1;
  if (rem < len)
      return -1;
  strcpy (p, ctx->seed);
  p += len;
  rem -= len;
  dat.dptr  = buf;
  dat.dsize = p - buf;
  return dbm_store (dbm, key, dat, DBM_REPLACE);
}
