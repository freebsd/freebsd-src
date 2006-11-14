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

RCSID("$Id: build_auth.c,v 1.38 2002/09/04 16:26:04 joda Exp $");

krb5_error_code
krb5_build_authenticator (krb5_context context,
			  krb5_auth_context auth_context,
			  krb5_enctype enctype,
			  krb5_creds *cred,
			  Checksum *cksum,
			  Authenticator **auth_result,
			  krb5_data *result,
			  krb5_key_usage usage)
{
  Authenticator *auth;
  u_char *buf = NULL;
  size_t buf_size;
  size_t len;
  krb5_error_code ret;
  krb5_crypto crypto;

  auth = malloc(sizeof(*auth));
  if (auth == NULL) {
      krb5_set_error_string(context, "malloc: out of memory");
      return ENOMEM;
  }

  memset (auth, 0, sizeof(*auth));
  auth->authenticator_vno = 5;
  copy_Realm(&cred->client->realm, &auth->crealm);
  copy_PrincipalName(&cred->client->name, &auth->cname);

  {
      int32_t sec, usec;

      krb5_us_timeofday (context, &sec, &usec);
      auth->ctime = sec;
      auth->cusec = usec;
  }
  ret = krb5_auth_con_getlocalsubkey(context, auth_context, &auth->subkey);
  if(ret)
      goto fail;

  if (auth_context->flags & KRB5_AUTH_CONTEXT_DO_SEQUENCE) {
    krb5_generate_seq_number (context,
			      &cred->session, 
			      &auth_context->local_seqnumber);
    ALLOC(auth->seq_number, 1);
    *auth->seq_number = auth_context->local_seqnumber;
  } else
    auth->seq_number = NULL;
  auth->authorization_data = NULL;
  auth->cksum = cksum;

  /* XXX - Copy more to auth_context? */

  if (auth_context) {
    auth_context->authenticator->ctime = auth->ctime;
    auth_context->authenticator->cusec = auth->cusec;
  }

  ASN1_MALLOC_ENCODE(Authenticator, buf, buf_size, auth, &len, ret);

  if (ret)
      goto fail;

  ret = krb5_crypto_init(context, &cred->session, enctype, &crypto);
  if (ret)
      goto fail;
  ret = krb5_encrypt (context,
		      crypto,
		      usage /* KRB5_KU_AP_REQ_AUTH */,
		      buf + buf_size - len, 
		      len,
		      result);
  krb5_crypto_destroy(context, crypto);

  if (ret)
      goto fail;

  free (buf);

  if (auth_result)
    *auth_result = auth;
  else {
    /* Don't free the `cksum', it's allocated by the caller */
    auth->cksum = NULL;
    free_Authenticator (auth);
    free (auth);
  }
  return ret;
fail:
  free_Authenticator (auth);
  free (auth);
  free (buf);
  return ret;
}
