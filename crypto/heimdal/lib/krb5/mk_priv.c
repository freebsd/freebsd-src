/*
 * Copyright (c) 1997 - 2002 Kungliga Tekniska Högskolan
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

#include <krb5_locl.h>

RCSID("$Id: mk_priv.c,v 1.31 2002/09/04 16:26:04 joda Exp $");

      
krb5_error_code
krb5_mk_priv(krb5_context context,
	     krb5_auth_context auth_context,
	     const krb5_data *userdata,
	     krb5_data *outbuf,
	     /*krb5_replay_data*/ void *outdata)
{
  krb5_error_code ret;
  KRB_PRIV s;
  EncKrbPrivPart part;
  u_char *buf;
  size_t buf_size;
  size_t len;
  u_int32_t tmp_seq;
  krb5_keyblock *key;
  int32_t sec, usec;
  KerberosTime sec2;
  int usec2;
  krb5_crypto crypto;

  if (auth_context->local_subkey)
      key = auth_context->local_subkey;
  else if (auth_context->remote_subkey)
      key = auth_context->remote_subkey;
  else
      key = auth_context->keyblock;

  krb5_us_timeofday (context, &sec, &usec);

  part.user_data = *userdata;
  sec2           = sec;
  part.timestamp = &sec2;
  usec2          = usec;
  part.usec      = &usec2;
  if (auth_context->flags & KRB5_AUTH_CONTEXT_DO_SEQUENCE) {
    tmp_seq = auth_context->local_seqnumber;
    part.seq_number = &tmp_seq;
  } else {
    part.seq_number = NULL;
  }

  part.s_address = auth_context->local_address;
  part.r_address = auth_context->remote_address;

  krb5_data_zero (&s.enc_part.cipher);

  ASN1_MALLOC_ENCODE(EncKrbPrivPart, buf, buf_size, &part, &len, ret);
  if (ret)
      goto fail;

  s.pvno = 5;
  s.msg_type = krb_priv;
  s.enc_part.etype = key->keytype;
  s.enc_part.kvno = NULL;

  ret = krb5_crypto_init(context, key, 0, &crypto);
  if (ret) {
      free (buf);
      return ret;
  }
  ret = krb5_encrypt (context, 
		      crypto,
		      KRB5_KU_KRB_PRIV,
		      buf + buf_size - len, 
		      len,
		      &s.enc_part.cipher);
  krb5_crypto_destroy(context, crypto);
  if (ret) {
      free(buf);
      return ret;
  }
  free(buf);


  ASN1_MALLOC_ENCODE(KRB_PRIV, buf, buf_size, &s, &len, ret);

  if(ret)
      goto fail;
  krb5_data_free (&s.enc_part.cipher);

  ret = krb5_data_copy(outbuf, buf + buf_size - len, len);
  if (ret) {
      krb5_set_error_string (context, "malloc: out of memory");
      free(buf);
      return ENOMEM;
  }
  free (buf);
  if (auth_context->flags & KRB5_AUTH_CONTEXT_DO_SEQUENCE)
      auth_context->local_seqnumber =
	  (auth_context->local_seqnumber + 1) & 0xFFFFFFFF;
  return 0;

fail:
  free (buf);
  krb5_data_free (&s.enc_part.cipher);
  return ret;
}
