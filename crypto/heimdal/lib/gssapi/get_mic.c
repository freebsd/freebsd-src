/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska Högskolan
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

RCSID("$Id: get_mic.c,v 1.11 2000/01/25 23:19:22 assar Exp $");

OM_uint32 gss_get_mic
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            gss_qop_t qop_req,
            const gss_buffer_t message_buffer,
            gss_buffer_t message_token
           )
{
  u_char *p;
  MD5_CTX md5;
  u_char hash[16];
  des_key_schedule schedule;
  des_cblock key;
  des_cblock zero;
  int32_t seq_number;
  size_t len, total_len;

  gssapi_krb5_encap_length (22, &len, &total_len);

  message_token->length = total_len;
  message_token->value  = malloc (total_len);
  if (message_token->value == NULL)
    return GSS_S_FAILURE;

  p = gssapi_krb5_make_header(message_token->value,
			      len,
			      "\x01\x01");

  memcpy (p, "\x00\x00", 2);
  p += 2;
  memcpy (p, "\xff\xff\xff\xff", 4);
  p += 4;

  /* Fill in later */
  memset (p, 0, 16);
  p += 16;

  /* checksum */
  MD5Init (&md5);
  MD5Update (&md5, p - 24, 8);
  MD5Update (&md5, message_buffer->value,
	     message_buffer->length);
  MD5Final (hash, &md5);

  memset (&zero, 0, sizeof(zero));
  gss_krb5_getsomekey(context_handle, &key);
  des_set_key (&key, schedule);
  des_cbc_cksum ((const void *)hash, (void *)hash, sizeof(hash),
		 schedule, &zero);
  memcpy (p - 8, hash, 8);

  /* sequence number */
  krb5_auth_getlocalseqnumber (gssapi_krb5_context,
			       context_handle->auth_context,
			       &seq_number);

  p -= 16;
  p[0] = (seq_number >> 0)  & 0xFF;
  p[1] = (seq_number >> 8)  & 0xFF;
  p[2] = (seq_number >> 16) & 0xFF;
  p[3] = (seq_number >> 24) & 0xFF;
  memset (p + 4,
	  (context_handle->more_flags & LOCAL) ? 0 : 0xFF,
	  4);

  des_set_key (&key, schedule);
  des_cbc_encrypt ((const void *)p, (void *)p, 8,
		   schedule, (des_cblock *)(p + 8), DES_ENCRYPT);

  krb5_auth_setlocalseqnumber (gssapi_krb5_context,
			       context_handle->auth_context,
			       ++seq_number);
  
  memset (key, 0, sizeof(key));
  memset (schedule, 0, sizeof(schedule));
  
  return GSS_S_COMPLETE;
}
