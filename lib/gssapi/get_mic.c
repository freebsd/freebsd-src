/*
 * Copyright (c) 1997 - 2003 Kungliga Tekniska Högskolan
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

#include "gssapi_locl.h"

RCSID("$Id: get_mic.c,v 1.21.2.1 2003/09/18 22:05:12 lha Exp $");

static OM_uint32
mic_des
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            gss_qop_t qop_req,
            const gss_buffer_t message_buffer,
            gss_buffer_t message_token,
	    krb5_keyblock *key
           )
{
  u_char *p;
  MD5_CTX md5;
  u_char hash[16];
  des_key_schedule schedule;
  des_cblock deskey;
  des_cblock zero;
  int32_t seq_number;
  size_t len, total_len;

  gssapi_krb5_encap_length (22, &len, &total_len);

  message_token->length = total_len;
  message_token->value  = malloc (total_len);
  if (message_token->value == NULL) {
    *minor_status = ENOMEM;
    return GSS_S_FAILURE;
  }

  p = gssapi_krb5_make_header(message_token->value,
			      len,
			      "\x01\x01"); /* TOK_ID */

  memcpy (p, "\x00\x00", 2);	/* SGN_ALG = DES MAC MD5 */
  p += 2;

  memcpy (p, "\xff\xff\xff\xff", 4); /* Filler */
  p += 4;

  /* Fill in later (SND-SEQ) */
  memset (p, 0, 16);
  p += 16;

  /* checksum */
  MD5_Init (&md5);
  MD5_Update (&md5, p - 24, 8);
  MD5_Update (&md5, message_buffer->value, message_buffer->length);
  MD5_Final (hash, &md5);

  memset (&zero, 0, sizeof(zero));
  memcpy (&deskey, key->keyvalue.data, sizeof(deskey));
  des_set_key (&deskey, schedule);
  des_cbc_cksum ((void *)hash, (void *)hash, sizeof(hash),
		 schedule, &zero);
  memcpy (p - 8, hash, 8);	/* SGN_CKSUM */

  /* sequence number */
  krb5_auth_con_getlocalseqnumber (gssapi_krb5_context,
			       context_handle->auth_context,
			       &seq_number);

  p -= 16;			/* SND_SEQ */
  p[0] = (seq_number >> 0)  & 0xFF;
  p[1] = (seq_number >> 8)  & 0xFF;
  p[2] = (seq_number >> 16) & 0xFF;
  p[3] = (seq_number >> 24) & 0xFF;
  memset (p + 4,
	  (context_handle->more_flags & LOCAL) ? 0 : 0xFF,
	  4);

  des_set_key (&deskey, schedule);
  des_cbc_encrypt ((void *)p, (void *)p, 8,
		   schedule, (des_cblock *)(p + 8), DES_ENCRYPT);

  krb5_auth_con_setlocalseqnumber (gssapi_krb5_context,
			       context_handle->auth_context,
			       ++seq_number);
  
  memset (deskey, 0, sizeof(deskey));
  memset (schedule, 0, sizeof(schedule));
  
  *minor_status = 0;
  return GSS_S_COMPLETE;
}

static OM_uint32
mic_des3
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            gss_qop_t qop_req,
            const gss_buffer_t message_buffer,
            gss_buffer_t message_token,
	    krb5_keyblock *key
           )
{
  u_char *p;
  Checksum cksum;
  u_char seq[8];

  int32_t seq_number;
  size_t len, total_len;

  krb5_crypto crypto;
  krb5_error_code kret;
  krb5_data encdata;
  char *tmp;
  char ivec[8];

  gssapi_krb5_encap_length (36, &len, &total_len);

  message_token->length = total_len;
  message_token->value  = malloc (total_len);
  if (message_token->value == NULL) {
      *minor_status = ENOMEM;
      return GSS_S_FAILURE;
  }

  p = gssapi_krb5_make_header(message_token->value,
			      len,
			      "\x01\x01"); /* TOK-ID */

  memcpy (p, "\x04\x00", 2);	/* SGN_ALG = HMAC SHA1 DES3-KD */
  p += 2;

  memcpy (p, "\xff\xff\xff\xff", 4); /* filler */
  p += 4;

  /* this should be done in parts */

  tmp = malloc (message_buffer->length + 8);
  if (tmp == NULL) {
      free (message_token->value);
      *minor_status = ENOMEM;
      return GSS_S_FAILURE;
  }
  memcpy (tmp, p - 8, 8);
  memcpy (tmp + 8, message_buffer->value, message_buffer->length);

  kret = krb5_crypto_init(gssapi_krb5_context, key, 0, &crypto);
  if (kret) {
      free (message_token->value);
      free (tmp);
      gssapi_krb5_set_error_string ();
      *minor_status = kret;
      return GSS_S_FAILURE;
  }

  kret = krb5_create_checksum (gssapi_krb5_context,
			       crypto,
			       KRB5_KU_USAGE_SIGN,
			       0,
			       tmp,
			       message_buffer->length + 8,
			       &cksum);
  free (tmp);
  krb5_crypto_destroy (gssapi_krb5_context, crypto);
  if (kret) {
      free (message_token->value);
      gssapi_krb5_set_error_string ();
      *minor_status = kret;
      return GSS_S_FAILURE;
  }

  memcpy (p + 8, cksum.checksum.data, cksum.checksum.length);

  /* sequence number */
  krb5_auth_con_getlocalseqnumber (gssapi_krb5_context,
			       context_handle->auth_context,
			       &seq_number);

  seq[0] = (seq_number >> 0)  & 0xFF;
  seq[1] = (seq_number >> 8)  & 0xFF;
  seq[2] = (seq_number >> 16) & 0xFF;
  seq[3] = (seq_number >> 24) & 0xFF;
  memset (seq + 4,
	  (context_handle->more_flags & LOCAL) ? 0 : 0xFF,
	  4);

  kret = krb5_crypto_init(gssapi_krb5_context, key,
			  ETYPE_DES3_CBC_NONE, &crypto);
  if (kret) {
      free (message_token->value);
      gssapi_krb5_set_error_string ();
      *minor_status = kret;
      return GSS_S_FAILURE;
  }

  if (context_handle->more_flags & COMPAT_OLD_DES3)
      memset(ivec, 0, 8);
  else
      memcpy(ivec, p + 8, 8);

  kret = krb5_encrypt_ivec (gssapi_krb5_context,
			    crypto,
			    KRB5_KU_USAGE_SEQ,
			    seq, 8, &encdata, ivec);
  krb5_crypto_destroy (gssapi_krb5_context, crypto);
  if (kret) {
      free (message_token->value);
      gssapi_krb5_set_error_string ();
      *minor_status = kret;
      return GSS_S_FAILURE;
  }
  
  assert (encdata.length == 8);

  memcpy (p, encdata.data, encdata.length);
  krb5_data_free (&encdata);

  krb5_auth_con_setlocalseqnumber (gssapi_krb5_context,
			       context_handle->auth_context,
			       ++seq_number);
  
  free_Checksum (&cksum);
  *minor_status = 0;
  return GSS_S_COMPLETE;
}

OM_uint32 gss_get_mic
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            gss_qop_t qop_req,
            const gss_buffer_t message_buffer,
            gss_buffer_t message_token
           )
{
  krb5_keyblock *key;
  OM_uint32 ret;
  krb5_keytype keytype;

  ret = gss_krb5_get_localkey(context_handle, &key);
  if (ret) {
      gssapi_krb5_set_error_string ();
      *minor_status = ret;
      return GSS_S_FAILURE;
  }
  krb5_enctype_to_keytype (gssapi_krb5_context, key->keytype, &keytype);

  switch (keytype) {
  case KEYTYPE_DES :
      ret = mic_des (minor_status, context_handle, qop_req,
		     message_buffer, message_token, key);
      break;
  case KEYTYPE_DES3 :
      ret = mic_des3 (minor_status, context_handle, qop_req,
		      message_buffer, message_token, key);
      break;
  case KEYTYPE_ARCFOUR:
      ret = _gssapi_get_mic_arcfour (minor_status, context_handle, qop_req,
				     message_buffer, message_token, key);
      break;
  default :
      *minor_status = KRB5_PROG_ETYPE_NOSUPP;
      ret = GSS_S_FAILURE;
      break;
  }
  krb5_free_keyblock (gssapi_krb5_context, key);
  return ret;
}
