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

RCSID("$Id: mk_req_ext.c,v 1.25 2001/05/09 07:15:00 assar Exp $");

krb5_error_code
krb5_mk_req_internal(krb5_context context,
		     krb5_auth_context *auth_context,
		     const krb5_flags ap_req_options,
		     krb5_data *in_data,
		     krb5_creds *in_creds,
		     krb5_data *outbuf,
		     krb5_key_usage checksum_usage,
		     krb5_key_usage encrypt_usage)
{
  krb5_error_code ret;
  krb5_data authenticator;
  Checksum c;
  Checksum *c_opt;
  krb5_auth_context ac;

  if(auth_context) {
      if(*auth_context == NULL)
	  ret = krb5_auth_con_init(context, auth_context);
      else
	  ret = 0;
      ac = *auth_context;
  } else
      ret = krb5_auth_con_init(context, &ac);
  if(ret)
      return ret;
      
#if 0
  {
      /* This is somewhat bogus since we're possibly overwriting a
         value specified by the user, but it's the easiest way to make
         the code use a compatible enctype */
      Ticket ticket;
      krb5_keytype ticket_keytype;

      ret = decode_Ticket(in_creds->ticket.data, 
			  in_creds->ticket.length, 
			  &ticket, 
			  NULL);
      krb5_enctype_to_keytype (context,
			       ticket.enc_part.etype,
			       &ticket_keytype);

      if (ticket_keytype == in_creds->session.keytype)
	  krb5_auth_setenctype(context, 
			       ac,
			       ticket.enc_part.etype);
      free_Ticket(&ticket);
  }
#endif

  krb5_free_keyblock(context, ac->keyblock);
  krb5_copy_keyblock(context, &in_creds->session, &ac->keyblock);
  
  /* it's unclear what type of checksum we can use.  try the best one, except:
   * a) if it's configured differently for the current realm, or
   * b) if the session key is des-cbc-crc
   */

  if (in_data) {
      if(ac->keyblock->keytype == ETYPE_DES_CBC_CRC) {
	  /* this is to make DCE secd (and older MIT kdcs?) happy */
	  ret = krb5_create_checksum(context, 
				     NULL,
				     0,
				     CKSUMTYPE_RSA_MD4,
				     in_data->data,
				     in_data->length,
				     &c);
      } else {
	  krb5_crypto crypto;

	  ret = krb5_crypto_init(context, ac->keyblock, 0, &crypto);
	  if (ret)
	      return ret;
	  ret = krb5_create_checksum(context, 
				     crypto,
				     checksum_usage,
				     0,
				     in_data->data,
				     in_data->length,
				     &c);
      
	  krb5_crypto_destroy(context, crypto);
      }
      c_opt = &c;
  } else {
      c_opt = NULL;
  }
  
  ret = krb5_build_authenticator (context,
				  ac,
				  ac->keyblock->keytype,
				  in_creds,
				  c_opt,
				  NULL,
				  &authenticator,
				  encrypt_usage);
  if (c_opt)
      free_Checksum (c_opt);
  if (ret)
    return ret;

  ret = krb5_build_ap_req (context, ac->keyblock->keytype, 
			   in_creds, ap_req_options, authenticator, outbuf);
  if(auth_context == NULL)
      krb5_auth_con_free(context, ac);
  return ret;
}

krb5_error_code
krb5_mk_req_extended(krb5_context context,
		     krb5_auth_context *auth_context,
		     const krb5_flags ap_req_options,
		     krb5_data *in_data,
		     krb5_creds *in_creds,
		     krb5_data *outbuf)
{
    return krb5_mk_req_internal (context,
				 auth_context,
				 ap_req_options,
				 in_data,
				 in_creds,
				 outbuf,
				 KRB5_KU_AP_REQ_AUTH_CKSUM,
				 KRB5_KU_AP_REQ_AUTH);
}
