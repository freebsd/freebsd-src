/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
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

RCSID("$Id: rd_safe.c,v 1.24 2001/05/14 06:14:51 assar Exp $");

static krb5_error_code
verify_checksum(krb5_context context,
		krb5_auth_context auth_context,
		KRB_SAFE *safe)
{
    krb5_error_code ret;
    u_char *buf;
    size_t buf_size;
    size_t len;
    Checksum c;
    krb5_crypto crypto;

    c = safe->cksum;
    safe->cksum.cksumtype       = 0;
    safe->cksum.checksum.data   = NULL;
    safe->cksum.checksum.length = 0;


    buf_size = length_KRB_SAFE(safe);
    buf = malloc(buf_size);

    if (buf == NULL) {
	ret = ENOMEM;
	krb5_set_error_string (context, "malloc: out of memory");
	goto out;
    }

    ret = encode_KRB_SAFE (buf + buf_size - 1,
			   buf_size,
			   safe,
			   &len);
    ret = krb5_crypto_init(context, auth_context->keyblock, 0, &crypto);
    if (ret)
	goto out;
    ret = krb5_verify_checksum (context,
				crypto,
				KRB5_KU_KRB_SAFE_CKSUM,
				buf + buf_size - len,
				len,
				&c);
    krb5_crypto_destroy(context, crypto);
out:
    safe->cksum = c;
    free (buf);
    return ret;
}

krb5_error_code
krb5_rd_safe(krb5_context context,
	     krb5_auth_context auth_context,
	     const krb5_data *inbuf,
	     krb5_data *outbuf,
	     /*krb5_replay_data*/ void *outdata)
{
  krb5_error_code ret;
  KRB_SAFE safe;
  size_t len;

  ret = decode_KRB_SAFE (inbuf->data, inbuf->length, &safe, &len);
  if (ret) 
      return ret;
  if (safe.pvno != 5) {
      ret = KRB5KRB_AP_ERR_BADVERSION;
      krb5_clear_error_string (context);
      goto failure;
  }
  if (safe.msg_type != krb_safe) {
      ret = KRB5KRB_AP_ERR_MSG_TYPE;
      krb5_clear_error_string (context);
      goto failure;
  }
  if (!krb5_checksum_is_keyed(context, safe.cksum.cksumtype)
      || !krb5_checksum_is_collision_proof(context, safe.cksum.cksumtype)) {
      ret = KRB5KRB_AP_ERR_INAPP_CKSUM;
      krb5_clear_error_string (context);
      goto failure;
  }

  /* check sender address */

  if (safe.safe_body.s_address
      && auth_context->remote_address
      && !krb5_address_compare (context,
				auth_context->remote_address,
				safe.safe_body.s_address)) {
      ret = KRB5KRB_AP_ERR_BADADDR;
      krb5_clear_error_string (context);
      goto failure;
  }

  /* check receiver address */

  if (safe.safe_body.r_address
      && auth_context->local_address
      && !krb5_address_compare (context,
				auth_context->local_address,
				safe.safe_body.r_address)) {
      ret = KRB5KRB_AP_ERR_BADADDR;
      krb5_clear_error_string (context);
      goto failure;
  }

  /* check timestamp */
  if (auth_context->flags & KRB5_AUTH_CONTEXT_DO_TIME) {
      krb5_timestamp sec;

      krb5_timeofday (context, &sec);

      if (safe.safe_body.timestamp == NULL ||
	  safe.safe_body.usec      == NULL ||
	  abs(*safe.safe_body.timestamp - sec) > context->max_skew) {
	  ret = KRB5KRB_AP_ERR_SKEW;
	  krb5_clear_error_string (context);
	  goto failure;
      }
  }
  /* XXX - check replay cache */

  /* check sequence number. since MIT krb5 cannot generate a sequence
     number of zero but instead generates no sequence number, we accept that
  */

  if (auth_context->flags & KRB5_AUTH_CONTEXT_DO_SEQUENCE) {
      if ((safe.safe_body.seq_number == NULL
	   && auth_context->remote_seqnumber != 0)
	  || (safe.safe_body.seq_number != NULL
	      && *safe.safe_body.seq_number !=
	      auth_context->remote_seqnumber)) {
	  ret = KRB5KRB_AP_ERR_BADORDER;
	  krb5_clear_error_string (context);
	  goto failure;
      }
      auth_context->remote_seqnumber++;
  }

  ret = verify_checksum (context, auth_context, &safe);
  if (ret)
      goto failure;
  
  outbuf->length = safe.safe_body.user_data.length;
  outbuf->data   = malloc(outbuf->length);
  if (outbuf->data == NULL) {
      ret = ENOMEM;
      krb5_set_error_string (context, "malloc: out of memory");
      goto failure;
  }
  memcpy (outbuf->data, safe.safe_body.user_data.data, outbuf->length);
  free_KRB_SAFE (&safe);
  return 0;
failure:
  free_KRB_SAFE (&safe);
  return ret;
}
