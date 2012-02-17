/*
 * Copyright (c) 1997, 1998 Kungliga Tekniska Högskolan
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

RCSID("$Id: get_in_tkt_with_skey.c,v 1.3 1999/12/02 17:05:10 joda Exp $");

static krb5_error_code
krb5_skey_key_proc (krb5_context context,
		    krb5_enctype type,
		    krb5_salt salt,
		    krb5_const_pointer keyseed,
		    krb5_keyblock **key)
{
    return krb5_copy_keyblock (context, keyseed, key);
}

krb5_error_code
krb5_get_in_tkt_with_skey (krb5_context context,
			   krb5_flags options,
			   krb5_addresses *addrs,
			   const krb5_enctype *etypes,
			   const krb5_preauthtype *pre_auth_types,
			   const krb5_keyblock *key,
			   krb5_ccache ccache,
			   krb5_creds *creds,
			   krb5_kdc_rep *ret_as_reply)
{
    if(key == NULL)
	return krb5_get_in_tkt_with_keytab (context,
					    options,
					    addrs,
					    etypes,
					    pre_auth_types,
					    NULL,
					    ccache,
					    creds,
					    ret_as_reply);
    else
	return krb5_get_in_tkt (context,
				options,
				addrs,
				etypes,
				pre_auth_types,
				krb5_skey_key_proc,
				key,
				NULL,
				NULL,
				creds,
				ccache,
				ret_as_reply);
}
