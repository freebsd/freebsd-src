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

#ifdef HAVE_CONFIG_H
#include "config.h"
RCSID("$Id$");
#endif

#include "otp_locl.h"

int
otp_verify_user_1 (OtpContext *ctx, const char *passwd)
{
  OtpKey key1, key2;

  if (otp_parse (key1, passwd, ctx->alg)) {
    ctx->err = "Syntax error in reply";
    return -1;
  }
  memcpy (key2, key1, sizeof(key1));
  ctx->alg->next (key2);
  if (memcmp (ctx->key, key2, sizeof(key2)) == 0) {
    --ctx->n;
    memcpy (ctx->key, key1, sizeof(key1));
    return 0;
  } else
    return -1;
}

int
otp_verify_user (OtpContext *ctx, const char *passwd)
{
  void *dbm;
  int ret;

  if (!ctx->challengep)
    return -1;
  ret = otp_verify_user_1 (ctx, passwd);
  dbm = otp_db_open ();
  if (dbm == NULL) {
    free(ctx->user);
    return -1;
  }
  otp_put (dbm, ctx);
  free(ctx->user);
  otp_db_close (dbm);
  return ret;
}
