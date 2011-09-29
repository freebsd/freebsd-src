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

RCSID("$Id: verify_mic.c,v 1.18.2.4 2003/09/18 22:05:34 lha Exp $");

static OM_uint32
verify_mic_des
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            const gss_buffer_t message_buffer,
            const gss_buffer_t token_buffer,
            gss_qop_t * qop_state,
	    krb5_keyblock *key,
	    char *type
	    )
{
  u_char *p;
  MD5_CTX md5;
  u_char hash[16], seq_data[8];
  des_key_schedule schedule;
  des_cblock zero;
  des_cblock deskey;
  int32_t seq_number;
  OM_uint32 ret;

  p = token_buffer->value;
  ret = gssapi_krb5_verify_header (&p,
				   token_buffer->length,
				   type);
  if (ret)
      return ret;

  if (memcmp(p, "\x00\x00", 2) != 0)
      return GSS_S_BAD_SIG;
  p += 2;
  if (memcmp (p, "\xff\xff\xff\xff", 4) != 0)
    return GSS_S_BAD_MIC;
  p += 4;
  p += 16;

  /* verify checksum */
  MD5_Init (&md5);
  MD5_Update (&md5, p - 24, 8);
  MD5_Update (&md5, message_buffer->value,
	     message_buffer->length);
  MD5_Final (hash, &md5);

  memset (&zero, 0, sizeof(zero));
  memcpy (&deskey, key->keyvalue.data, sizeof(deskey));

  des_set_key (&deskey, schedule);
  des_cbc_cksum ((void *)hash, (void *)hash, sizeof(hash),
		 schedule, &zero);
  if (memcmp (p - 8, hash, 8) != 0) {
    memset (deskey, 0, sizeof(deskey));
    memset (schedule, 0, sizeof(schedule));
    return GSS_S_BAD_MIC;
  }

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

  return GSS_S_COMPLETE;
}

static OM_uint32
verify_mic_des3
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            const gss_buffer_t message_buffer,
            const gss_buffer_t token_buffer,
            gss_qop_t * qop_state,
	    krb5_keyblock *key,
	    char *type
	    )
{
  u_char *p;
  u_char seq[8];
  int32_t seq_number;
  OM_uint32 ret;
  krb5_crypto crypto;
  krb5_data seq_data;
  int cmp, docompat;
  Checksum csum;
  char *tmp;
  char ivec[8];
  
  p = token_buffer->value;
  ret = gssapi_krb5_verify_header (&p,
				   token_buffer->length,
				   type);
  if (ret)
      return ret;

  if (memcmp(p, "\x04\x00", 2) != 0) /* SGN_ALG = HMAC SHA1 DES3-KD */
      return GSS_S_BAD_SIG;
  p += 2;
  if (memcmp (p, "\xff\xff\xff\xff", 4) != 0)
    return GSS_S_BAD_MIC;
  p += 4;

  ret = krb5_crypto_init(gssapi_krb5_context, key,
			 ETYPE_DES3_CBC_NONE, &crypto);
  if (ret){
      gssapi_krb5_set_error_string ();
      *minor_status = ret;
      return GSS_S_FAILURE;
  }

  /* verify sequence number */
  docompat = 0;
retry:
  if (docompat)
      memset(ivec, 0, 8);
  else
      memcpy(ivec, p + 8, 8);

  ret = krb5_decrypt_ivec (gssapi_krb5_context,
			   crypto,
			   KRB5_KU_USAGE_SEQ,
			   p, 8, &seq_data, ivec);
  if (ret) {
      if (docompat++) {
	  gssapi_krb5_set_error_string ();
	  krb5_crypto_destroy (gssapi_krb5_context, crypto);
	  *minor_status = ret;
	  return GSS_S_FAILURE;
      } else
	  goto retry;
  }

  if (seq_data.length != 8) {
      krb5_data_free (&seq_data);
      if (docompat++) {
	  krb5_crypto_destroy (gssapi_krb5_context, crypto);
	  return GSS_S_BAD_MIC;
      } else
	  goto retry;
  }

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
  cmp = memcmp (seq, seq_data.data, seq_data.length);
  krb5_data_free (&seq_data);
  if (cmp != 0) {
      if (docompat++) {
	  krb5_crypto_destroy (gssapi_krb5_context, crypto);
	  return GSS_S_BAD_MIC;
      } else
	  goto retry;
  }

  /* verify checksum */

  tmp = malloc (message_buffer->length + 8);
  if (tmp == NULL) {
      krb5_crypto_destroy (gssapi_krb5_context, crypto);
      *minor_status = ENOMEM;
      return GSS_S_FAILURE;
  }

  memcpy (tmp, p - 8, 8);
  memcpy (tmp + 8, message_buffer->value, message_buffer->length);

  csum.cksumtype = CKSUMTYPE_HMAC_SHA1_DES3;
  csum.checksum.length = 20;
  csum.checksum.data   = p + 8;

  ret = krb5_verify_checksum (gssapi_krb5_context, crypto,
			      KRB5_KU_USAGE_SIGN,
			      tmp, message_buffer->length + 8,
			      &csum);
  free (tmp);
  if (ret) {
      gssapi_krb5_set_error_string ();
      krb5_crypto_destroy (gssapi_krb5_context, crypto);
      *minor_status = ret;
      return GSS_S_BAD_MIC;
  }

  krb5_auth_con_setremoteseqnumber (gssapi_krb5_context,
				context_handle->auth_context,
				++seq_number);

  krb5_crypto_destroy (gssapi_krb5_context, crypto);
  return GSS_S_COMPLETE;
}

OM_uint32
gss_verify_mic_internal
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            const gss_buffer_t message_buffer,
            const gss_buffer_t token_buffer,
            gss_qop_t * qop_state,
	    char * type
	    )
{
    krb5_keyblock *key;
    OM_uint32 ret;
    krb5_keytype keytype;

    ret = gss_krb5_get_remotekey(context_handle, &key);
    if (ret) {
	gssapi_krb5_set_error_string ();
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
    *minor_status = 0;
    krb5_enctype_to_keytype (gssapi_krb5_context, key->keytype, &keytype);
    switch (keytype) {
    case KEYTYPE_DES :
	ret = verify_mic_des (minor_status, context_handle,
			      message_buffer, token_buffer, qop_state, key,
			      type);
	break;
    case KEYTYPE_DES3 :
	ret = verify_mic_des3 (minor_status, context_handle,
			       message_buffer, token_buffer, qop_state, key,
			       type);
	break;
    case KEYTYPE_ARCFOUR :
	ret = _gssapi_verify_mic_arcfour (minor_status, context_handle,
					  message_buffer, token_buffer,
					  qop_state, key, type);
	break;
    default :
	*minor_status = KRB5_PROG_ETYPE_NOSUPP;
	ret = GSS_S_FAILURE;
	break;
    }
    krb5_free_keyblock (gssapi_krb5_context, key);
    
    return ret;
}

OM_uint32
gss_verify_mic
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            const gss_buffer_t message_buffer,
            const gss_buffer_t token_buffer,
            gss_qop_t * qop_state
	    )
{
    OM_uint32 ret;

    if (qop_state != NULL)
	*qop_state = GSS_C_QOP_DEFAULT;

    ret = gss_verify_mic_internal(minor_status, context_handle, 
				  message_buffer, token_buffer,
				  qop_state, "\x01\x01");

    return ret;
}
