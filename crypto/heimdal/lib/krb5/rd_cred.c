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

#include <krb5_locl.h>

RCSID("$Id: rd_cred.c,v 1.9 2000/02/06 05:19:52 assar Exp $");

krb5_error_code
krb5_rd_cred (krb5_context      context,
	      krb5_auth_context auth_context,
	      krb5_ccache       ccache,
	      krb5_data         *in_data)
{
    krb5_error_code ret;
    size_t len;
    KRB_CRED cred;
    EncKrbCredPart enc_krb_cred_part;
    krb5_data enc_krb_cred_part_data;
    krb5_crypto crypto;
    int i;

    ret = decode_KRB_CRED (in_data->data, in_data->length,
			   &cred, &len);
    if (ret)
	return ret;

    if (cred.pvno != 5) {
	ret = KRB5KRB_AP_ERR_BADVERSION;
	goto out;
    }

    if (cred.msg_type != krb_cred) {
	ret = KRB5KRB_AP_ERR_MSG_TYPE;
	goto out;
    }

    krb5_crypto_init(context, auth_context->remote_subkey, 0, &crypto);
    ret = krb5_decrypt_EncryptedData(context,
				     crypto,
				     KRB5_KU_KRB_CRED,
				     &cred.enc_part,
				     &enc_krb_cred_part_data);
    krb5_crypto_destroy(context, crypto);
    if (ret)
	goto out;
    

    ret = krb5_decode_EncKrbCredPart (context,
				      enc_krb_cred_part_data.data,
				      enc_krb_cred_part_data.length,
				      &enc_krb_cred_part,
				      &len);
    if (ret)
	goto out;

    /* check sender address */

    if (enc_krb_cred_part.s_address
	&& auth_context->remote_address) {
	krb5_address *a;
	int cmp;

	ret = krb5_make_addrport (&a,
				  auth_context->remote_address,
				  auth_context->remote_port);
	if (ret)
	    goto out;


	cmp = krb5_address_compare (context,
				    a,
				    enc_krb_cred_part.s_address);

	krb5_free_address (context, a);
	free (a);

	if (cmp == 0) {
	    ret = KRB5KRB_AP_ERR_BADADDR;
	    goto out;
	}
    }

    /* check receiver address */

    if (enc_krb_cred_part.r_address
	&& !krb5_address_compare (context,
				  auth_context->local_address,
				  enc_krb_cred_part.r_address)) {
	ret = KRB5KRB_AP_ERR_BADADDR;
	goto out;
    }

    /* check timestamp */
    if (auth_context->flags & KRB5_AUTH_CONTEXT_DO_TIME) {
	krb5_timestamp sec;

	krb5_timeofday (context, &sec);

	if (enc_krb_cred_part.timestamp == NULL ||
	    enc_krb_cred_part.usec      == NULL ||
	    abs(*enc_krb_cred_part.timestamp - sec)
	    > context->max_skew) {
	    ret = KRB5KRB_AP_ERR_SKEW;
	    goto out;
	}
    }

    /* XXX - check replay cache */

    /* Store the creds in the ccache */

    for (i = 0; i < enc_krb_cred_part.ticket_info.len; ++i) {
	KrbCredInfo *kci = &enc_krb_cred_part.ticket_info.val[i];
	krb5_creds creds;
	u_char buf[1024];
	size_t len;

	memset (&creds, 0, sizeof(creds));

	ret = encode_Ticket (buf + sizeof(buf) - 1, sizeof(buf),
			     &cred.tickets.val[i],
			     &len);
	if (ret)
	    goto out;
	krb5_data_copy (&creds.ticket, buf + sizeof(buf) - len, len);
	copy_EncryptionKey (&kci->key, &creds.session);
	if (kci->prealm && kci->pname)
	    principalname2krb5_principal (&creds.client,
					  *kci->pname,
					  *kci->prealm);
	if (kci->flags)
	    creds.flags.b = *kci->flags;
	if (kci->authtime)
	    creds.times.authtime = *kci->authtime;
	if (kci->starttime)
	    creds.times.starttime = *kci->starttime;
	if (kci->endtime)
	    creds.times.endtime = *kci->endtime;
	if (kci->renew_till)
	    creds.times.renew_till = *kci->renew_till;
	if (kci->srealm && kci->sname)
	    principalname2krb5_principal (&creds.server,
					  *kci->sname,
					  *kci->srealm);
	if (kci->caddr)
	    krb5_copy_addresses (context,
				 kci->caddr,
				 &creds.addresses);
	krb5_cc_store_cred (context, ccache, &creds);
    }

out:
    free_KRB_CRED (&cred);
    return ret;
}
