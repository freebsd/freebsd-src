/*
 * Copyright (c) 1997 - 1999 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"

RCSID("$Id: get_in_tkt_pw.c,v 1.15 1999/12/02 17:05:10 joda Exp $");

krb5_error_code
krb5_password_key_proc (krb5_context context,
			krb5_enctype type,
			krb5_salt salt,
			krb5_const_pointer keyseed,
			krb5_keyblock **key)
{
    krb5_error_code ret;
    const char *password = (const char *)keyseed;
    char buf[BUFSIZ];
     
    *key = malloc (sizeof (**key));
    if (*key == NULL)
	return ENOMEM;
    if (password == NULL) {
	if(des_read_pw_string (buf, sizeof(buf), "Password: ", 0)) {
	    free (*key);
	    return KRB5_LIBOS_PWDINTR;
	}
	password = buf;
    }
    ret = krb5_string_to_key_salt (context, type, password, salt, *key);
    memset (buf, 0, sizeof(buf));
    return ret;
}

krb5_error_code
krb5_get_in_tkt_with_password (krb5_context context,
			       krb5_flags options,
			       krb5_addresses *addrs,
			       const krb5_enctype *etypes,
			       const krb5_preauthtype *pre_auth_types,
			       const char *password,
			       krb5_ccache ccache,
			       krb5_creds *creds,
			       krb5_kdc_rep *ret_as_reply)
{
     return krb5_get_in_tkt (context,
			     options,
			     addrs,
			     etypes,
			     pre_auth_types,
			     krb5_password_key_proc,
			     password,
			     NULL,
			     NULL,
			     creds,
			     ccache,
			     ret_as_reply);
}
