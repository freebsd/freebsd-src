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

RCSID("$Id: get_in_tkt_with_keytab.c,v 1.5 1999/12/02 17:05:10 joda Exp $");

krb5_error_code
krb5_keytab_key_proc (krb5_context context,
		      krb5_enctype enctype,
		      krb5_salt salt,
		      krb5_const_pointer keyseed,
		      krb5_keyblock **key)
{
    krb5_keytab_key_proc_args *args  = (krb5_keytab_key_proc_args *)keyseed;
    krb5_keytab keytab = args->keytab;
    krb5_principal principal  = args->principal;
    krb5_error_code ret;
    krb5_keytab real_keytab;
    krb5_keytab_entry entry;

    if(keytab == NULL)
	krb5_kt_default(context, &real_keytab);
    else
	real_keytab = keytab;

    ret = krb5_kt_get_entry (context, real_keytab, principal,
			     0, enctype, &entry);

    if (keytab == NULL)
	krb5_kt_close (context, real_keytab);

    if (ret)
	return ret;

    ret = krb5_copy_keyblock (context, &entry.keyblock, key);
    krb5_kt_free_entry(context, &entry);
    return ret;
}

krb5_error_code
krb5_get_in_tkt_with_keytab (krb5_context context,
			     krb5_flags options,
			     krb5_addresses *addrs,
			     const krb5_enctype *etypes,
			     const krb5_preauthtype *pre_auth_types,
			     krb5_keytab keytab,
			     krb5_ccache ccache,
			     krb5_creds *creds,
			     krb5_kdc_rep *ret_as_reply)
{
    krb5_keytab_key_proc_args *a;

    a = malloc(sizeof(*a));
    if (a == NULL)
	return ENOMEM;

    a->principal = creds->client;
    a->keytab    = keytab;

    return krb5_get_in_tkt (context,
			    options,
			    addrs,
			    etypes,
			    pre_auth_types,
			    krb5_keytab_key_proc,
			    a,
			    NULL,
			    NULL,
			    creds,
			    ccache,
			    ret_as_reply);
}
