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

#include "gssapi_locl.h"

RCSID("$Id: unwrap.c,v 1.21 2002/09/03 17:33:11 joda Exp $");

OM_uint32
gss_krb5_get_remotekey(const gss_ctx_id_t context_handle,
		       krb5_keyblock **key)
{
    krb5_keyblock *skey;

    krb5_auth_con_getremotesubkey(gssapi_krb5_context,
				  context_handle->auth_context, 
				  &skey);
    if(skey == NULL)
	krb5_auth_con_getlocalsubkey(gssapi_krb5_context,
				     context_handle->auth_context, 
				     &skey);
    if(skey == NULL)
	krb5_auth_con_getkey(gssapi_krb5_context,
			     context_handle->auth_context, 
			     &skey);
    if(skey == NULL)
	return GSS_S_FAILURE;
    *key = skey;
    return 0;
}

static OM_uint32
unwrap_des
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            const gss_buffer_t input_message_buffer,
            gss_buffer_t output_message_buffer,
            int * conf_state,
            gss_qop_t * qop_state,
	    krb5_keyblock *key
           )
{
  u_char *p, *pad;
  size_t len;
  MD5_CTX md5;
  u_char hash[16], seq_data[8];
  des_key_schedule schedule;
  des_cblock deskey;
  des_cblock zero;
  int i;
  int32_t seq_number;
  size_t padlength;
  OM_uint32 ret;
  int cstate;

  p = input_message_buffer->value;
  ret = gssapi_krb5_verify_header (&p,
				   input_message_buffer->length,
				   "\x02\x01");
  if (ret) {
      *minor_status = 0;
      return ret;
  }

  if (memcmp (p, "\x00\x00", 2) != 0)
    return GSS_S_BAD_SIG;
  p += 2;
  if (memcmp (p, "\x00\x00", 2) == 0) {
      cstate = 1;
  } else if (memcmp (p, "\xFF\xFF", 2) == 0) {
      cstate = 0;
  } else
      return GSS_S_BAD_MIC;
  p += 2;
  if(conf_state != NULL)
      *conf_state = cstate;
  if (memcmp (p, "\xff\xff", 2) != 0)
    return GSS_S_DEFECTIVE_TOKEN;
  p += 2;
  p += 16;

  len = p - (u_char *)input_message_buffer->value;

  if(cstate) {
      /* decrypt data */
      memcpy (&deskey, key->keyvalue.data, sizeof(deskey));

      for (i = 0; i < sizeof(deskey); ++i)
	  deskey[i] ^= 0xf0;
      des_set_key (&deskey, schedule);
      memset (&zero, 0, sizeof(zero));
      des_cbc_encrypt ((void *)p,
		       (void *)p,
		       input_message_buffer->length - len,
		       schedule,
		       &zero,
		       DES_DECRYPT);
      
      memset (deskey, 0, sizeof(deskey));
      memset (schedule, 0, sizeof(schedule));
  }
  /* check pad */

  pad = (u_char *)input_message_buffer->value + input_message_buffer->length - 1;
  padlength = *pad;

  for (i = padlength; i > 0 && *pad == padlength; i--, pad--)
    ;
  if (i != 0)
    return GSS_S_BAD_MIC;

  MD5_Init (&md5);
  MD5_Update (&md5, p - 24, 8);
  MD5_Update (&md5, p, input_message_buffer->length - len);
  MD5_Final (hash, &md5);

  memset (&zero, 0, sizeof(zero));
  memcpy (&deskey, key->keyvalue.data, sizeof(deskey));
  des_set_key (&deskey, schedule);
  des_cbc_cksum ((void *)hash, (void *)hash, sizeof(hash),
		 schedule, &zero);
  if (memcmp (p - 8, hash, 8) != 0)
    return GSS_S_BAD_MIC;

  /* verify sequence number */
  
  krb5_auth_getremoteseqnumber (gssapi_krb5_context,
				context_handle->auth_context,
				&seq_number);
  seq_data[0] = (seq_number >> 0)  & 0xFF;
  seq_data[1] = (seq_number >> 8)  & 0xFF;
  seq_data[2] = (seq_number >> 16) & 0xFF;
  seq_data[3] = (seq_number >> 24) & 0xFF;
  memset (seq_data + 4,
	  (context_handle->more_flags & LOCAL) ? 0xFF : 0,
	  4);

  p -= 16;
  des_set_key (&deskey, schedule);
  des_cbc_encrypt ((void *)p, (void *)p, 8,
		   schedule, (des_cblock *)hash, DES_DECRYPT);

  memset (deskey, 0, sizeof(deskey));
  memset (schedule, 0, sizeof(schedule));

  if (memcmp (p, seq_data, 8) != 0) {
    return GSS_S_BAD_MIC;
  }

  krb5_auth_con_setremoteseqnumber (gssapi_krb5_context,
				context_handle->auth_context,
				++seq_number);

  /* copy out data */

  output_message_buffer->length = input_message_buffer->length
    - len - padlength - 8;
  output_message_buffer->value  = malloc(output_message_buffer->length);
  if(output_message_buffer->length != 0 && output_message_buffer->value == NULL)
      return GSS_S_FAILURE;
  memcpy (output_message_buffer->value,
	  p + 24,
	  output_message_buffer->length);
  return GSS_S_COMPLETE;
}

static OM_uint32
unwrap_des3
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            const gss_buffer_t input_message_buffer,
            gss_buffer_t output_message_buffer,
            int * conf_state,
            gss_qop_t * qop_state,
	    krb5_keyblock *key
           )
{
  u_char *p, *pad;
  size_t len;
  u_char seq[8];
  krb5_data seq_data;
  u_char cksum[20];
  int i;
  int32_t seq_number;
  size_t padlength;
  OM_uint32 ret;
  int cstate;
  krb5_crypto crypto;
  Checksum csum;
  int cmp;

  p = input_message_buffer->value;
  ret = gssapi_krb5_verify_header (&p,
				   input_message_buffer->length,
				   "\x02\x01");
  if (ret) {
      *minor_status = 0;
      return ret;
  }

  if (memcmp (p, "\x04\x00", 2) != 0) /* HMAC SHA1 DES3_KD */
    return GSS_S_BAD_SIG;
  p += 2;
  if (memcmp (p, "\x02\x00", 2) == 0) {
    cstate = 1;
  } else if (memcmp (p, "\xff\xff", 2) == 0) {
    cstate = 0;
  } else
    return GSS_S_BAD_MIC;
  p += 2;
  if(conf_state != NULL)
    *conf_state = cstate;
  if (memcmp (p, "\xff\xff", 2) != 0)
    return GSS_S_DEFECTIVE_TOKEN;
  p += 2;
  p += 28;

  len = p - (u_char *)input_message_buffer->value;

  if(cstate) {
      /* decrypt data */
      krb5_data tmp;

      ret = krb5_crypto_init(gssapi_krb5_context, key,
			     ETYPE_DES3_CBC_NONE, &crypto);
      if (ret) {
	  gssapi_krb5_set_error_string ();
	  *minor_status = ret;
	  return GSS_S_FAILURE;
      }
      ret = krb5_decrypt(gssapi_krb5_context, crypto, KRB5_KU_USAGE_SEAL,
			 p, input_message_buffer->length - len, &tmp);
      krb5_crypto_destroy(gssapi_krb5_context, crypto);
      if (ret) {
	  gssapi_krb5_set_error_string ();
	  *minor_status = ret;
	  return GSS_S_FAILURE;
      }
      assert (tmp.length == input_message_buffer->length - len);

      memcpy (p, tmp.data, tmp.length);
      krb5_data_free(&tmp);
  }
  /* check pad */

  pad = (u_char *)input_message_buffer->value + input_message_buffer->length - 1;
  padlength = *pad;

  for (i = padlength; i > 0 && *pad == padlength; i--, pad--)
    ;
  if (i != 0)
    return GSS_S_BAD_MIC;

  /* verify sequence number */
  
  krb5_auth_getremoteseqnumber (gssapi_krb5_context,
				context_handle->auth_context,
				&seq_number);
  seq[0] = (seq_number >> 0)  & 0xFF;
  seq[1] = (seq_number >> 8)  & 0xFF;
  seq[2] = (seq_number >> 16) & 0xFF;
  seq[3] = (seq_number >> 24) & 0xFF;
  memset (seq + 4,
	  (context_handle->more_flags & LOCAL) ? 0xFF : 0,
	  4);

  p -= 28;

  ret = krb5_crypto_init(gssapi_krb5_context, key,
			 ETYPE_DES3_CBC_NONE, &crypto);
  if (ret) {
      gssapi_krb5_set_error_string ();
      *minor_status = ret;
      return GSS_S_FAILURE;
  }
  {
      des_cblock ivec;

      memcpy(&ivec, p + 8, 8);
      ret = krb5_decrypt_ivec (gssapi_krb5_context,
			       crypto,
			       KRB5_KU_USAGE_SEQ,
			       p, 8, &seq_data,
			       &ivec);
  }
  krb5_crypto_destroy (gssapi_krb5_context, crypto);
  if (ret) {
      gssapi_krb5_set_error_string ();
      *minor_status = ret;
      return GSS_S_FAILURE;
  }
  if (seq_data.length != 8) {
      krb5_data_free (&seq_data);
      return GSS_S_BAD_MIC;
  }

  cmp = memcmp (seq, seq_data.data, seq_data.length);
  krb5_data_free (&seq_data);
  if (cmp != 0) {
      return GSS_S_BAD_MIC;
  }

  krb5_auth_con_setremoteseqnumber (gssapi_krb5_context,
				context_handle->auth_context,
				++seq_number);

  /* verify checksum */

  memcpy (cksum, p + 8, 20);

  memcpy (p + 20, p - 8, 8);

  csum.cksumtype = CKSUMTYPE_HMAC_SHA1_DES3;
  csum.checksum.length = 20;
  csum.checksum.data   = cksum;

  ret = krb5_crypto_init(gssapi_krb5_context, key, 0, &crypto);
  if (ret) {
      gssapi_krb5_set_error_string ();
      *minor_status = ret;
      return GSS_S_FAILURE;
  }

  ret = krb5_verify_checksum (gssapi_krb5_context, crypto,
			      KRB5_KU_USAGE_SIGN,
			      p + 20,
			      input_message_buffer->length - len + 8,
			      &csum);
  krb5_crypto_destroy (gssapi_krb5_context, crypto);
  if (ret) {
      gssapi_krb5_set_error_string ();
      *minor_status = ret;
      return GSS_S_FAILURE;
  }

  /* copy out data */

  output_message_buffer->length = input_message_buffer->length
    - len - padlength - 8;
  output_message_buffer->value  = malloc(output_message_buffer->length);
  if(output_message_buffer->length != 0 && output_message_buffer->value == NULL)
      return GSS_S_FAILURE;
  memcpy (output_message_buffer->value,
	  p + 36,
	  output_message_buffer->length);
  return GSS_S_COMPLETE;
}

OM_uint32 gss_unwrap
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            const gss_buffer_t input_message_buffer,
            gss_buffer_t output_message_buffer,
            int * conf_state,
            gss_qop_t * qop_state
           )
{
  krb5_keyblock *key;
  OM_uint32 ret;
  krb5_keytype keytype;

  if (qop_state != NULL)
      *qop_state = GSS_C_QOP_DEFAULT;
  ret = gss_krb5_get_remotekey(context_handle, &key);
  if (ret) {
      gssapi_krb5_set_error_string ();
      *minor_status = ret;
      return GSS_S_FAILURE;
  }
  krb5_enctype_to_keytype (gssapi_krb5_context, key->keytype, &keytype);

  switch (keytype) {
  case KEYTYPE_DES :
      ret = unwrap_des (minor_status, context_handle,
			input_message_buffer, output_message_buffer,
			conf_state, qop_state, key);
      break;
  case KEYTYPE_DES3 :
      ret = unwrap_des3 (minor_status, context_handle,
			 input_message_buffer, output_message_buffer,
			 conf_state, qop_state, key);
      break;
  default :
      *minor_status = KRB5_PROG_ETYPE_NOSUPP;
      ret = GSS_S_FAILURE;
      break;
  }
  krb5_free_keyblock (gssapi_krb5_context, key);
  return ret;
}
