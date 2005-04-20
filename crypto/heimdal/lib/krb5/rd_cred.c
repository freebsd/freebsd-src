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

RCSID("$Id: rd_cred.c,v 1.18 2002/09/04 16:26:05 joda Exp $");

krb5_error_code
krb5_rd_cred(krb5_context context,
	     krb5_auth_context auth_context,
	     krb5_data *in_data,
	     krb5_creds ***ret_creds,
	     krb5_replay_data *out_data)
{
    krb5_error_code ret;
    size_t len;
    KRB_CRED cred;
    EncKrbCredPart enc_krb_cred_part;
    krb5_data enc_krb_cred_part_data;
    krb5_crypto crypto;
    int i;

    *ret_creds = NULL;

    ret = decode_KRB_CRED(in_data->data, in_data->length, 
			  &cred, &len);
    if(ret)
	return ret;

    if (cred.pvno != 5) {
	ret = KRB5KRB_AP_ERR_BADVERSION;
	krb5_clear_error_string (context);
	goto out;
    }

    if (cred.msg_type != krb_cred) {
	ret = KRB5KRB_AP_ERR_MSG_TYPE;
	krb5_clear_error_string (context);
	goto out;
    }

    if (cred.enc_part.etype == ETYPE_NULL) {  
       /* DK: MIT GSS-API Compatibility */
       enc_krb_cred_part_data.length = cred.enc_part.cipher.length;
       enc_krb_cred_part_data.data   = cred.enc_part.cipher.data;
    } else {
	if (auth_context->remote_subkey)
	    ret = krb5_crypto_init(context, auth_context->remote_subkey,
				   0, &crypto);
	else
	    ret = krb5_crypto_init(context, auth_context->keyblock,
				   0, &crypto);
          /* DK: MIT rsh */

	if (ret)
	    goto out;
       
	ret = krb5_decrypt_EncryptedData(context,
					 crypto,
					 KRB5_KU_KRB_CRED,
					 &cred.enc_part,
					 &enc_krb_cred_part_data);
       
	krb5_crypto_destroy(context, crypto);
	if (ret)
	    goto out;
    }

    ret = krb5_decode_EncKrbCredPart (context,
				      enc_krb_cred_part_data.data,
				      enc_krb_cred_part_data.length,
				      &enc_krb_cred_part,
				      &len);
    if (ret)
	goto out;

    /* check sender address */

    if (enc_krb_cred_part.s_address
	&& auth_context->remote_address
	&& auth_context->remote_port) {
	krb5_address *a;
	int cmp;

	ret = krb5_make_addrport (context, &a,
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
	    krb5_clear_error_string (context);
	    ret = KRB5KRB_AP_ERR_BADADDR;
	    goto out;
	}
    }

    /* check receiver address */

    if (enc_krb_cred_part.r_address
	&& auth_context->local_address) {
	if(auth_context->local_port &&
	   enc_krb_cred_part.r_address->addr_type == KRB5_ADDRESS_ADDRPORT) {
	    krb5_address *a;
	    int cmp;
	    ret = krb5_make_addrport (context, &a,
				      auth_context->local_address,
				      auth_context->local_port);
	    if (ret)
		goto out;
	    
	    cmp = krb5_address_compare (context,
					a,
					enc_krb_cred_part.r_address);
	    krb5_free_address (context, a);
	    free (a);
	    
	    if (cmp == 0) {
		krb5_clear_error_string (context);
		ret = KRB5KRB_AP_ERR_BADADDR;
		goto out;
	    }
	} else {
	    if(!krb5_address_compare (context,
				      auth_context->local_address,
				      enc_krb_cred_part.r_address)) {
		krb5_clear_error_string (context);
		ret = KRB5KRB_AP_ERR_BADADDR;
		goto out;
	    }		
	}
    }

    /* check timestamp */
    if (auth_context->flags & KRB5_AUTH_CONTEXT_DO_TIME) {
	krb5_timestamp sec;

	krb5_timeofday (context, &sec);

	if (enc_krb_cred_part.timestamp == NULL ||
	    enc_krb_cred_part.usec      == NULL ||
	    abs(*enc_krb_cred_part.timestamp - sec)
	    > context->max_skew) {
	    krb5_clear_error_string (context);
	    ret = KRB5KRB_AP_ERR_SKEW;
	    goto out;
	}
    }

    if(out_data != NULL) {
	if(enc_krb_cred_part.timestamp)
	    out_data->timestamp = *enc_krb_cred_part.timestamp;
	else 
	    out_data->timestamp = 0;
	if(enc_krb_cred_part.usec)
	    out_data->usec = *enc_krb_cred_part.usec;
	else 
	    out_data->usec = 0;
	if(enc_krb_cred_part.nonce)
	    out_data->seq = *enc_krb_cred_part.nonce;
	else 
	    out_data->seq = 0;
    }
    
    /* Convert to NULL terminated list of creds */

    *ret_creds = calloc(enc_krb_cred_part.ticket_info.len + 1, 
		       sizeof(**ret_creds));

    if (*ret_creds == NULL) {
	ret = ENOMEM;
	krb5_set_error_string (context, "malloc: out of memory");
	goto out;
    }

    for (i = 0; i < enc_krb_cred_part.ticket_info.len; ++i) {
	KrbCredInfo *kci = &enc_krb_cred_part.ticket_info.val[i];
	krb5_creds *creds;
	size_t len;

	creds = calloc(1, sizeof(*creds));
	if(creds == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_string (context, "malloc: out of memory");
	    goto out;
	}

	ASN1_MALLOC_ENCODE(Ticket, creds->ticket.data, creds->ticket.length, 
			   &cred.tickets.val[i], &len, ret);
	if (ret)
	    goto out;
	if(creds->ticket.length != len)
	    krb5_abortx(context, "internal error in ASN.1 encoder");
	copy_EncryptionKey (&kci->key, &creds->session);
	if (kci->prealm && kci->pname)
	    principalname2krb5_principal (&creds->client,
					  *kci->pname,
					  *kci->prealm);
	if (kci->flags)
	    creds->flags.b = *kci->flags;
	if (kci->authtime)
	    creds->times.authtime = *kci->authtime;
	if (kci->starttime)
	    creds->times.starttime = *kci->starttime;
	if (kci->endtime)
	    creds->times.endtime = *kci->endtime;
	if (kci->renew_till)
	    creds->times.renew_till = *kci->renew_till;
	if (kci->srealm && kci->sname)
	    principalname2krb5_principal (&creds->server,
					  *kci->sname,
					  *kci->srealm);
	if (kci->caddr)
	    krb5_copy_addresses (context,
				 kci->caddr,
				 &creds->addresses);
	
	(*ret_creds)[i] = creds;
	
    }
    (*ret_creds)[i] = NULL;
    return 0;

out:
    free_KRB_CRED (&cred);
    if(*ret_creds) {
	for(i = 0; (*ret_creds)[i]; i++)
	    krb5_free_creds(context, (*ret_creds)[i]);
	free(*ret_creds);
    }
    return ret;
}

krb5_error_code
krb5_rd_cred2 (krb5_context      context,
	       krb5_auth_context auth_context,
	       krb5_ccache       ccache,
	       krb5_data         *in_data)
{
    krb5_error_code ret;
    krb5_creds **creds;
    int i;

    ret = krb5_rd_cred(context, auth_context, in_data, &creds, NULL);
    if(ret)
	return ret;

    /* Store the creds in the ccache */

    for(i = 0; creds && creds[i]; i++) {
	krb5_cc_store_cred(context, ccache, creds[i]);
	krb5_free_creds(context, creds[i]);
    }
    free(creds);
    return 0;
}
