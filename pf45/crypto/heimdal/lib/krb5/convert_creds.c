/*
 * Copyright (c) 1997 - 2004 Kungliga Tekniska Högskolan
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
RCSID("$Id: convert_creds.c 22050 2007-11-11 11:20:46Z lha $");

#include "krb5-v4compat.h"

static krb5_error_code
check_ticket_flags(TicketFlags f)
{
    return 0; /* maybe add some more tests here? */
}

/**
 * Convert the v5 credentials in in_cred to v4-dito in v4creds.  This
 * is done by sending them to the 524 function in the KDC.  If
 * `in_cred' doesn't contain a DES session key, then a new one is
 * gotten from the KDC and stored in the cred cache `ccache'.
 *
 * @param context Kerberos 5 context.
 * @param in_cred the credential to convert
 * @param v4creds the converted credential
 *
 * @return Returns 0 to indicate success. Otherwise an kerberos et
 * error code is returned, see krb5_get_error_message().
 *
 * @ingroup krb5_v4compat
 */

krb5_error_code KRB5_LIB_FUNCTION
krb524_convert_creds_kdc(krb5_context context, 
			 krb5_creds *in_cred,
			 struct credentials *v4creds)
{
    krb5_error_code ret;
    krb5_data reply;
    krb5_storage *sp;
    int32_t tmp;
    krb5_data ticket;
    char realm[REALM_SZ];
    krb5_creds *v5_creds = in_cred;

    ret = check_ticket_flags(v5_creds->flags.b);
    if(ret)
	goto out2;

    {
	krb5_krbhst_handle handle;

	ret = krb5_krbhst_init(context,
			       krb5_principal_get_realm(context, 
							v5_creds->server),
			       KRB5_KRBHST_KRB524,
			       &handle);
	if (ret)
	    goto out2;

	ret = krb5_sendto (context,
			   &v5_creds->ticket,
			   handle,
			   &reply);
	krb5_krbhst_free(context, handle);
	if (ret)
	    goto out2;
    }
    sp = krb5_storage_from_mem(reply.data, reply.length);
    if(sp == NULL) {
	ret = ENOMEM;
	krb5_set_error_string (context, "malloc: out of memory");
	goto out2;
    }
    krb5_ret_int32(sp, &tmp);
    ret = tmp;
    if(ret == 0) {
	memset(v4creds, 0, sizeof(*v4creds));
	ret = krb5_ret_int32(sp, &tmp);
	if(ret)
	    goto out;
	v4creds->kvno = tmp;
	ret = krb5_ret_data(sp, &ticket);
	if(ret)
	    goto out;
	v4creds->ticket_st.length = ticket.length;
	memcpy(v4creds->ticket_st.dat, ticket.data, ticket.length);
	krb5_data_free(&ticket);
	ret = krb5_524_conv_principal(context, 
				      v5_creds->server, 
				      v4creds->service, 
				      v4creds->instance, 
				      v4creds->realm);
	if(ret)
	    goto out;
	v4creds->issue_date = v5_creds->times.starttime;
	v4creds->lifetime = _krb5_krb_time_to_life(v4creds->issue_date,
						   v5_creds->times.endtime);
	ret = krb5_524_conv_principal(context, v5_creds->client, 
				      v4creds->pname, 
				      v4creds->pinst, 
				      realm);
	if(ret)
	    goto out;
	memcpy(v4creds->session, v5_creds->session.keyvalue.data, 8);
    } else {
	krb5_set_error_string(context, "converting credentials: %s", 
			      krb5_get_err_text(context, ret));
    }
out:
    krb5_storage_free(sp);
    krb5_data_free(&reply);
out2:
    if (v5_creds != in_cred)
	krb5_free_creds (context, v5_creds);
    return ret;
}

/**
 * Convert the v5 credentials in in_cred to v4-dito in v4creds,
 * check the credential cache ccache before checking with the KDC.
 *
 * @param context Kerberos 5 context.
 * @param ccache credential cache used to check for des-ticket.
 * @param in_cred the credential to convert
 * @param v4creds the converted credential
 *
 * @return Returns 0 to indicate success. Otherwise an kerberos et
 * error code is returned, see krb5_get_error_message().
 *
 * @ingroup krb5_v4compat
 */

krb5_error_code KRB5_LIB_FUNCTION
krb524_convert_creds_kdc_ccache(krb5_context context, 
				krb5_ccache ccache,
				krb5_creds *in_cred,
				struct credentials *v4creds)
{
    krb5_error_code ret;
    krb5_creds *v5_creds = in_cred;
    krb5_keytype keytype;

    keytype = v5_creds->session.keytype;

    if (keytype != ENCTYPE_DES_CBC_CRC) {
	/* MIT krb524d doesn't like nothing but des-cbc-crc tickets,
           so go get one */
	krb5_creds template;

	memset (&template, 0, sizeof(template));
	template.session.keytype = ENCTYPE_DES_CBC_CRC;
	ret = krb5_copy_principal (context, in_cred->client, &template.client);
	if (ret) {
	    krb5_free_cred_contents (context, &template);
	    return ret;
	}
	ret = krb5_copy_principal (context, in_cred->server, &template.server);
	if (ret) {
	    krb5_free_cred_contents (context, &template);
	    return ret;
	}

	ret = krb5_get_credentials (context, 0, ccache,
				    &template, &v5_creds);
	krb5_free_cred_contents (context, &template);
	if (ret)
	    return ret;
    }

    ret = krb524_convert_creds_kdc(context, v5_creds, v4creds);

    if (v5_creds != in_cred)
	krb5_free_creds (context, v5_creds);
    return ret;
}
