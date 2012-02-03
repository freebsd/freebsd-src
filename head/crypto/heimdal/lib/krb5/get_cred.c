/*
 * Copyright (c) 1997 - 2007 Kungliga Tekniska Högskolan
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

RCSID("$Id: get_cred.c 21668 2007-07-22 11:28:05Z lha $");

/*
 * Take the `body' and encode it into `padata' using the credentials
 * in `creds'.
 */

static krb5_error_code
make_pa_tgs_req(krb5_context context, 
		krb5_auth_context ac,
		KDC_REQ_BODY *body,
		PA_DATA *padata,
		krb5_creds *creds,
		krb5_key_usage usage)
{
    u_char *buf;
    size_t buf_size;
    size_t len;
    krb5_data in_data;
    krb5_error_code ret;

    ASN1_MALLOC_ENCODE(KDC_REQ_BODY, buf, buf_size, body, &len, ret);
    if (ret)
	goto out;
    if(buf_size != len)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    in_data.length = len;
    in_data.data   = buf;
    ret = _krb5_mk_req_internal(context, &ac, 0, &in_data, creds,
				&padata->padata_value,
				KRB5_KU_TGS_REQ_AUTH_CKSUM,
				usage
				/* KRB5_KU_TGS_REQ_AUTH */);
 out:
    free (buf);
    if(ret)
	return ret;
    padata->padata_type = KRB5_PADATA_TGS_REQ;
    return 0;
}

/*
 * Set the `enc-authorization-data' in `req_body' based on `authdata'
 */

static krb5_error_code
set_auth_data (krb5_context context,
	       KDC_REQ_BODY *req_body,
	       krb5_authdata *authdata,
	       krb5_keyblock *key)
{
    if(authdata->len) {
	size_t len, buf_size;
	unsigned char *buf;
	krb5_crypto crypto;
	krb5_error_code ret;

	ASN1_MALLOC_ENCODE(AuthorizationData, buf, buf_size, authdata,
			   &len, ret);
	if (ret)
	    return ret;
	if (buf_size != len)
	    krb5_abortx(context, "internal error in ASN.1 encoder");

	ALLOC(req_body->enc_authorization_data, 1);
	if (req_body->enc_authorization_data == NULL) {
	    free (buf);
	    krb5_set_error_string(context, "malloc: out of memory");
	    return ENOMEM;
	}
	ret = krb5_crypto_init(context, key, 0, &crypto);
	if (ret) {
	    free (buf);
	    free (req_body->enc_authorization_data);
	    req_body->enc_authorization_data = NULL;
	    return ret;
	}
	krb5_encrypt_EncryptedData(context, 
				   crypto,
				   KRB5_KU_TGS_REQ_AUTH_DAT_SUBKEY, 
				   /* KRB5_KU_TGS_REQ_AUTH_DAT_SESSION? */
				   buf,
				   len,
				   0,
				   req_body->enc_authorization_data);
	free (buf);
	krb5_crypto_destroy(context, crypto);
    } else {
	req_body->enc_authorization_data = NULL;
    }
    return 0;
}    

/*
 * Create a tgs-req in `t' with `addresses', `flags', `second_ticket'
 * (if not-NULL), `in_creds', `krbtgt', and returning the generated
 * subkey in `subkey'.
 */

static krb5_error_code
init_tgs_req (krb5_context context,
	      krb5_ccache ccache,
	      krb5_addresses *addresses,
	      krb5_kdc_flags flags,
	      Ticket *second_ticket,
	      krb5_creds *in_creds,
	      krb5_creds *krbtgt,
	      unsigned nonce,
	      const METHOD_DATA *padata,
	      krb5_keyblock **subkey,
	      TGS_REQ *t,
	      krb5_key_usage usage)
{
    krb5_error_code ret = 0;

    memset(t, 0, sizeof(*t));
    t->pvno = 5;
    t->msg_type = krb_tgs_req;
    if (in_creds->session.keytype) {
	ALLOC_SEQ(&t->req_body.etype, 1);
	if(t->req_body.etype.val == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_string(context, "malloc: out of memory");
	    goto fail;
	}
	t->req_body.etype.val[0] = in_creds->session.keytype;
    } else {
	ret = krb5_init_etype(context, 
			      &t->req_body.etype.len, 
			      &t->req_body.etype.val, 
			      NULL);
    }
    if (ret)
	goto fail;
    t->req_body.addresses = addresses;
    t->req_body.kdc_options = flags.b;
    ret = copy_Realm(&in_creds->server->realm, &t->req_body.realm);
    if (ret)
	goto fail;
    ALLOC(t->req_body.sname, 1);
    if (t->req_body.sname == NULL) {
	ret = ENOMEM;
	krb5_set_error_string(context, "malloc: out of memory");
	goto fail;
    }

    /* some versions of some code might require that the client be
       present in TGS-REQs, but this is clearly against the spec */

    ret = copy_PrincipalName(&in_creds->server->name, t->req_body.sname);
    if (ret)
	goto fail;

    /* req_body.till should be NULL if there is no endtime specified,
       but old MIT code (like DCE secd) doesn't like that */
    ALLOC(t->req_body.till, 1);
    if(t->req_body.till == NULL){
	ret = ENOMEM;
	krb5_set_error_string(context, "malloc: out of memory");
	goto fail;
    }
    *t->req_body.till = in_creds->times.endtime;
    
    t->req_body.nonce = nonce;
    if(second_ticket){
	ALLOC(t->req_body.additional_tickets, 1);
	if (t->req_body.additional_tickets == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_string(context, "malloc: out of memory");
	    goto fail;
	}
	ALLOC_SEQ(t->req_body.additional_tickets, 1);
	if (t->req_body.additional_tickets->val == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_string(context, "malloc: out of memory");
	    goto fail;
	}
	ret = copy_Ticket(second_ticket, t->req_body.additional_tickets->val); 
	if (ret)
	    goto fail;
    }
    ALLOC(t->padata, 1);
    if (t->padata == NULL) {
	ret = ENOMEM;
	krb5_set_error_string(context, "malloc: out of memory");
	goto fail;
    }
    ALLOC_SEQ(t->padata, 1 + padata->len);
    if (t->padata->val == NULL) {
	ret = ENOMEM;
	krb5_set_error_string(context, "malloc: out of memory");
	goto fail;
    }
    {
	int i;
	for (i = 0; i < padata->len; i++) {
	    ret = copy_PA_DATA(&padata->val[i], &t->padata->val[i + 1]);
	    if (ret) {
		krb5_set_error_string(context, "malloc: out of memory");
		goto fail;
	    }
	}
    }

    {
	krb5_auth_context ac;
	krb5_keyblock *key = NULL;

	ret = krb5_auth_con_init(context, &ac);
	if(ret)
	    goto fail;

	if (krb5_config_get_bool_default(context, NULL, FALSE,
					 "realms",
					 krbtgt->server->realm,
					 "tgs_require_subkey",
					 NULL))
	{
	    ret = krb5_generate_subkey (context, &krbtgt->session, &key);
	    if (ret) {
		krb5_auth_con_free (context, ac);
		goto fail;
	    }

	    ret = krb5_auth_con_setlocalsubkey(context, ac, key);
	    if (ret) {
		if (key)
		    krb5_free_keyblock (context, key);
		krb5_auth_con_free (context, ac);
		goto fail;
	    }
	}

	ret = set_auth_data (context, &t->req_body, &in_creds->authdata,
			     key ? key : &krbtgt->session);
	if (ret) {
	    if (key)
		krb5_free_keyblock (context, key);
	    krb5_auth_con_free (context, ac);
	    goto fail;
	}

	ret = make_pa_tgs_req(context,
			      ac,
			      &t->req_body, 
			      &t->padata->val[0],
			      krbtgt,
			      usage);
	if(ret) {
	    if (key)
		krb5_free_keyblock (context, key);
	    krb5_auth_con_free(context, ac);
	    goto fail;
	}
	*subkey = key;
	
	krb5_auth_con_free(context, ac);
    }
fail:
    if (ret) {
	t->req_body.addresses = NULL;
	free_TGS_REQ (t);
    }
    return ret;
}

krb5_error_code
_krb5_get_krbtgt(krb5_context context,
		 krb5_ccache  id,
		 krb5_realm realm,
		 krb5_creds **cred)
{
    krb5_error_code ret;
    krb5_creds tmp_cred;

    memset(&tmp_cred, 0, sizeof(tmp_cred));

    ret = krb5_cc_get_principal(context, id, &tmp_cred.client);
    if (ret)
	return ret;

    ret = krb5_make_principal(context, 
			      &tmp_cred.server,
			      realm,
			      KRB5_TGS_NAME,
			      realm,
			      NULL);
    if(ret) {
	krb5_free_principal(context, tmp_cred.client);
	return ret;
    }
    ret = krb5_get_credentials(context,
			       KRB5_GC_CACHED,
			       id,
			       &tmp_cred,
			       cred);
    krb5_free_principal(context, tmp_cred.client);
    krb5_free_principal(context, tmp_cred.server);
    if(ret)
	return ret;
    return 0;
}

/* DCE compatible decrypt proc */
static krb5_error_code
decrypt_tkt_with_subkey (krb5_context context,
			 krb5_keyblock *key,
			 krb5_key_usage usage,
			 krb5_const_pointer subkey,
			 krb5_kdc_rep *dec_rep)
{
    krb5_error_code ret;
    krb5_data data;
    size_t size;
    krb5_crypto crypto;
    
    ret = krb5_crypto_init(context, key, 0, &crypto);
    if (ret)
	return ret;
    ret = krb5_decrypt_EncryptedData (context,
				      crypto,
				      usage,
				      &dec_rep->kdc_rep.enc_part,
				      &data);
    krb5_crypto_destroy(context, crypto);
    if(ret && subkey){
	/* DCE compat -- try to decrypt with subkey */
	ret = krb5_crypto_init(context, subkey, 0, &crypto);
	if (ret)
	    return ret;
	ret = krb5_decrypt_EncryptedData (context,
					  crypto,
					  KRB5_KU_TGS_REP_ENC_PART_SUB_KEY,
					  &dec_rep->kdc_rep.enc_part,
					  &data);
	krb5_crypto_destroy(context, crypto);
    }
    if (ret)
	return ret;
    
    ret = krb5_decode_EncASRepPart(context,
				   data.data,
				   data.length,
				   &dec_rep->enc_part, 
				   &size);
    if (ret)
	ret = krb5_decode_EncTGSRepPart(context,
					data.data,
					data.length,
					&dec_rep->enc_part, 
					&size);
    krb5_data_free (&data);
    return ret;
}

static krb5_error_code
get_cred_kdc_usage(krb5_context context, 
		   krb5_ccache id, 
		   krb5_kdc_flags flags,
		   krb5_addresses *addresses, 
		   krb5_creds *in_creds,
		   krb5_creds *krbtgt,
		   krb5_principal impersonate_principal,
		   Ticket *second_ticket,
		   krb5_creds *out_creds,
		   krb5_key_usage usage)
{
    TGS_REQ req;
    krb5_data enc;
    krb5_data resp;
    krb5_kdc_rep rep;
    KRB_ERROR error;
    krb5_error_code ret;
    unsigned nonce;
    krb5_keyblock *subkey = NULL;
    size_t len;
    Ticket second_ticket_data;
    METHOD_DATA padata;
    
    krb5_data_zero(&resp);
    krb5_data_zero(&enc);
    padata.val = NULL;
    padata.len = 0;

    krb5_generate_random_block(&nonce, sizeof(nonce));
    nonce &= 0xffffffff;
    
    if(flags.b.enc_tkt_in_skey && second_ticket == NULL){
	ret = decode_Ticket(in_creds->second_ticket.data, 
			    in_creds->second_ticket.length, 
			    &second_ticket_data, &len);
	if(ret)
	    return ret;
	second_ticket = &second_ticket_data;
    }


    if (impersonate_principal) {
	krb5_crypto crypto;
	PA_S4U2Self self;
	krb5_data data;
	void *buf;
	size_t size;

	self.name = impersonate_principal->name;
	self.realm = impersonate_principal->realm;
	self.auth = estrdup("Kerberos");
	
	ret = _krb5_s4u2self_to_checksumdata(context, &self, &data);
	if (ret) {
	    free(self.auth);
	    goto out;
	}

	ret = krb5_crypto_init(context, &krbtgt->session, 0, &crypto);
	if (ret) {
	    free(self.auth);
	    krb5_data_free(&data);
	    goto out;
	}

	ret = krb5_create_checksum(context,
				   crypto,
				   KRB5_KU_OTHER_CKSUM,
				   0,
				   data.data,
				   data.length, 
				   &self.cksum);
	krb5_crypto_destroy(context, crypto);
	krb5_data_free(&data);
	if (ret) {
	    free(self.auth);
	    goto out;
	}

	ASN1_MALLOC_ENCODE(PA_S4U2Self, buf, len, &self, &size, ret);
	free(self.auth);
	free_Checksum(&self.cksum);
	if (ret)
	    goto out;
	if (len != size)
	    krb5_abortx(context, "internal asn1 error");
	
	ret = krb5_padata_add(context, &padata, KRB5_PADATA_S4U2SELF, buf, len);
	if (ret)
	    goto out;
    }

    ret = init_tgs_req (context,
			id,
			addresses,
			flags,
			second_ticket,
			in_creds,
			krbtgt,
			nonce,
			&padata,
			&subkey, 
			&req,
			usage);
    if (ret)
	goto out;

    ASN1_MALLOC_ENCODE(TGS_REQ, enc.data, enc.length, &req, &len, ret);
    if (ret) 
	goto out;
    if(enc.length != len)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    /* don't free addresses */
    req.req_body.addresses = NULL;
    free_TGS_REQ(&req);

    /*
     * Send and receive
     */
    {
	krb5_sendto_ctx stctx;
	ret = krb5_sendto_ctx_alloc(context, &stctx);
	if (ret)
	    return ret;
	krb5_sendto_ctx_set_func(stctx, _krb5_kdc_retry, NULL);

	ret = krb5_sendto_context (context, stctx, &enc,
				   krbtgt->server->name.name_string.val[1],
				   &resp);
	krb5_sendto_ctx_free(context, stctx);
    }
    if(ret)
	goto out;

    memset(&rep, 0, sizeof(rep));
    if(decode_TGS_REP(resp.data, resp.length, &rep.kdc_rep, &len) == 0){
	ret = krb5_copy_principal(context, 
				  in_creds->client, 
				  &out_creds->client);
	if(ret)
	    goto out;
	ret = krb5_copy_principal(context, 
				  in_creds->server, 
				  &out_creds->server);
	if(ret)
	    goto out;
	/* this should go someplace else */
	out_creds->times.endtime = in_creds->times.endtime;

	ret = _krb5_extract_ticket(context,
				   &rep,
				   out_creds,
				   &krbtgt->session,
				   NULL,
				   KRB5_KU_TGS_REP_ENC_PART_SESSION,
				   &krbtgt->addresses,
				   nonce,
				   EXTRACT_TICKET_ALLOW_CNAME_MISMATCH|
				   EXTRACT_TICKET_ALLOW_SERVER_MISMATCH,
				   decrypt_tkt_with_subkey,
				   subkey);
	krb5_free_kdc_rep(context, &rep);
    } else if(krb5_rd_error(context, &resp, &error) == 0) {
	ret = krb5_error_from_rd_error(context, &error, in_creds);
	krb5_free_error_contents(context, &error);
    } else if(resp.data && ((char*)resp.data)[0] == 4) {
	ret = KRB5KRB_AP_ERR_V4_REPLY;
	krb5_clear_error_string(context);
    } else {
	ret = KRB5KRB_AP_ERR_MSG_TYPE;
	krb5_clear_error_string(context);
    }

out:
    if (second_ticket == &second_ticket_data)
	free_Ticket(&second_ticket_data);
    free_METHOD_DATA(&padata);
    krb5_data_free(&resp);
    krb5_data_free(&enc);
    if(subkey){
	krb5_free_keyblock_contents(context, subkey);
	free(subkey);
    }
    return ret;
    
}

static krb5_error_code
get_cred_kdc(krb5_context context, 
	     krb5_ccache id, 
	     krb5_kdc_flags flags,
	     krb5_addresses *addresses, 
	     krb5_creds *in_creds, 
	     krb5_creds *krbtgt,
	     krb5_principal impersonate_principal,
	     Ticket *second_ticket,
	     krb5_creds *out_creds)
{
    krb5_error_code ret;

    ret = get_cred_kdc_usage(context, id, flags, addresses, in_creds,
			     krbtgt, impersonate_principal, second_ticket,
			     out_creds, KRB5_KU_TGS_REQ_AUTH);
    if (ret == KRB5KRB_AP_ERR_BAD_INTEGRITY) {
	krb5_clear_error_string (context);
	ret = get_cred_kdc_usage(context, id, flags, addresses, in_creds,
				 krbtgt, impersonate_principal, second_ticket,
				 out_creds, KRB5_KU_AP_REQ_AUTH);
    }
    return ret;
}

/* same as above, just get local addresses first */

static krb5_error_code
get_cred_kdc_la(krb5_context context, krb5_ccache id, krb5_kdc_flags flags, 
		krb5_creds *in_creds, krb5_creds *krbtgt, 
		krb5_principal impersonate_principal, Ticket *second_ticket,
		krb5_creds *out_creds)
{
    krb5_error_code ret;
    krb5_addresses addresses, *addrs = &addresses;
    
    krb5_get_all_client_addrs(context, &addresses);
    /* XXX this sucks. */
    if(addresses.len == 0)
	addrs = NULL;
    ret = get_cred_kdc(context, id, flags, addrs, 
		       in_creds, krbtgt, impersonate_principal, second_ticket,
		       out_creds);
    krb5_free_addresses(context, &addresses);
    return ret;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_kdc_cred(krb5_context context,
		  krb5_ccache id,
		  krb5_kdc_flags flags,
		  krb5_addresses *addresses,
		  Ticket  *second_ticket,
		  krb5_creds *in_creds,
		  krb5_creds **out_creds
		  )
{
    krb5_error_code ret;
    krb5_creds *krbtgt;

    *out_creds = calloc(1, sizeof(**out_creds));
    if(*out_creds == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    ret = _krb5_get_krbtgt (context,
			    id,
			    in_creds->server->realm,
			    &krbtgt);
    if(ret) {
	free(*out_creds);
	return ret;
    }
    ret = get_cred_kdc(context, id, flags, addresses, 
		       in_creds, krbtgt, NULL, NULL, *out_creds);
    krb5_free_creds (context, krbtgt);
    if(ret)
	free(*out_creds);
    return ret;
}

static void
not_found(krb5_context context, krb5_const_principal p)
{
    krb5_error_code ret;
    char *str;

    ret = krb5_unparse_name(context, p, &str);
    if(ret) {
	krb5_clear_error_string(context);
	return;
    }
    krb5_set_error_string(context, "Matching credential (%s) not found", str);
    free(str);
}

static krb5_error_code
find_cred(krb5_context context,
	  krb5_ccache id,
	  krb5_principal server,
	  krb5_creds **tgts,
	  krb5_creds *out_creds)
{
    krb5_error_code ret;
    krb5_creds mcreds;

    krb5_cc_clear_mcred(&mcreds);
    mcreds.server = server;
    ret = krb5_cc_retrieve_cred(context, id, KRB5_TC_DONT_MATCH_REALM, 
				&mcreds, out_creds);
    if(ret == 0)
	return 0;
    while(tgts && *tgts){
	if(krb5_compare_creds(context, KRB5_TC_DONT_MATCH_REALM, 
			      &mcreds, *tgts)){
	    ret = krb5_copy_creds_contents(context, *tgts, out_creds);
	    return ret;
	}
	tgts++;
    }
    not_found(context, server);
    return KRB5_CC_NOTFOUND;
}

static krb5_error_code
add_cred(krb5_context context, krb5_creds ***tgts, krb5_creds *tkt)
{
    int i;
    krb5_error_code ret;
    krb5_creds **tmp = *tgts;

    for(i = 0; tmp && tmp[i]; i++); /* XXX */
    tmp = realloc(tmp, (i+2)*sizeof(*tmp));
    if(tmp == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    *tgts = tmp;
    ret = krb5_copy_creds(context, tkt, &tmp[i]);
    tmp[i+1] = NULL;
    return ret;
}

/*
get_cred(server)
	creds = cc_get_cred(server)
	if(creds) return creds
	tgt = cc_get_cred(krbtgt/server_realm@any_realm)
	if(tgt)
		return get_cred_tgt(server, tgt)
	if(client_realm == server_realm)
		return NULL
	tgt = get_cred(krbtgt/server_realm@client_realm)
	while(tgt_inst != server_realm)
		tgt = get_cred(krbtgt/server_realm@tgt_inst)
	return get_cred_tgt(server, tgt)
	*/

static krb5_error_code
get_cred_from_kdc_flags(krb5_context context,
			krb5_kdc_flags flags,
			krb5_ccache ccache,
			krb5_creds *in_creds,
			krb5_principal impersonate_principal,
			Ticket *second_ticket,			
			krb5_creds **out_creds,
			krb5_creds ***ret_tgts)
{
    krb5_error_code ret;
    krb5_creds *tgt, tmp_creds;
    krb5_const_realm client_realm, server_realm, try_realm;

    *out_creds = NULL;

    client_realm = krb5_principal_get_realm(context, in_creds->client);
    server_realm = krb5_principal_get_realm(context, in_creds->server);
    memset(&tmp_creds, 0, sizeof(tmp_creds));
    ret = krb5_copy_principal(context, in_creds->client, &tmp_creds.client);
    if(ret)
	return ret;

    try_realm = krb5_config_get_string(context, NULL, "capaths", 
				       client_realm, server_realm, NULL);
    
#if 1
    /* XXX remove in future release */
    if(try_realm == NULL)
	try_realm = krb5_config_get_string(context, NULL, "libdefaults", 
					   "capath", server_realm, NULL);
#endif

    if (try_realm == NULL)
	try_realm = client_realm;

    ret = krb5_make_principal(context,
			      &tmp_creds.server,
			      try_realm,
			      KRB5_TGS_NAME,
			      server_realm, 
			      NULL);
    if(ret){
	krb5_free_principal(context, tmp_creds.client);
	return ret;
    }
    {
	krb5_creds tgts;
	/* XXX try krb5_cc_retrieve_cred first? */
	ret = find_cred(context, ccache, tmp_creds.server, 
			*ret_tgts, &tgts);
	if(ret == 0){
	    *out_creds = calloc(1, sizeof(**out_creds));
	    if(*out_creds == NULL) {
		krb5_set_error_string(context, "malloc: out of memory");
		ret = ENOMEM;
	    } else {
		krb5_boolean noaddr;

		krb5_appdefault_boolean(context, NULL, tgts.server->realm,
					"no-addresses", FALSE, &noaddr);

		if (noaddr)
		    ret = get_cred_kdc(context, ccache, flags, NULL,
				       in_creds, &tgts,
				       impersonate_principal, 
				       second_ticket,
				       *out_creds);
		else
		    ret = get_cred_kdc_la(context, ccache, flags, 
					  in_creds, &tgts, 
					  impersonate_principal, 
					  second_ticket,
					  *out_creds);
		if (ret) {
		    free (*out_creds);
		    *out_creds = NULL;
		}
	    }
	    krb5_free_cred_contents(context, &tgts);
	    krb5_free_principal(context, tmp_creds.server);
	    krb5_free_principal(context, tmp_creds.client);
	    return ret;
	}
    }
    if(krb5_realm_compare(context, in_creds->client, in_creds->server)) {
	not_found(context, in_creds->server);
	return KRB5_CC_NOTFOUND;
    }
    /* XXX this can loop forever */
    while(1){
	heim_general_string tgt_inst;

	ret = get_cred_from_kdc_flags(context, flags, ccache, &tmp_creds, 
				      NULL, NULL, &tgt, ret_tgts);
	if(ret) {
	    krb5_free_principal(context, tmp_creds.server);
	    krb5_free_principal(context, tmp_creds.client);
	    return ret;
	}
	ret = add_cred(context, ret_tgts, tgt);
	if(ret) {
	    krb5_free_principal(context, tmp_creds.server);
	    krb5_free_principal(context, tmp_creds.client);
	    return ret;
	}
	tgt_inst = tgt->server->name.name_string.val[1];
	if(strcmp(tgt_inst, server_realm) == 0)
	    break;
	krb5_free_principal(context, tmp_creds.server);
	ret = krb5_make_principal(context, &tmp_creds.server, 
				  tgt_inst, KRB5_TGS_NAME, server_realm, NULL);
	if(ret) {
	    krb5_free_principal(context, tmp_creds.server);
	    krb5_free_principal(context, tmp_creds.client);
	    return ret;
	}
	ret = krb5_free_creds(context, tgt);
	if(ret) {
	    krb5_free_principal(context, tmp_creds.server);
	    krb5_free_principal(context, tmp_creds.client);
	    return ret;
	}
    }
	
    krb5_free_principal(context, tmp_creds.server);
    krb5_free_principal(context, tmp_creds.client);
    *out_creds = calloc(1, sizeof(**out_creds));
    if(*out_creds == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
    } else {
	krb5_boolean noaddr;

	krb5_appdefault_boolean(context, NULL, tgt->server->realm,
				"no-addresses", KRB5_ADDRESSLESS_DEFAULT,
				&noaddr);
	if (noaddr)
	    ret = get_cred_kdc (context, ccache, flags, NULL,
				in_creds, tgt, NULL, NULL,
				*out_creds);
	else
	    ret = get_cred_kdc_la(context, ccache, flags, 
				  in_creds, tgt, NULL, NULL,
				  *out_creds);
	if (ret) {
	    free (*out_creds);
	    *out_creds = NULL;
	}
    }
    krb5_free_creds(context, tgt);
    return ret;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_cred_from_kdc_opt(krb5_context context,
			   krb5_ccache ccache,
			   krb5_creds *in_creds,
			   krb5_creds **out_creds,
			   krb5_creds ***ret_tgts,
			   krb5_flags flags)
{
    krb5_kdc_flags f;
    f.i = flags;
    return get_cred_from_kdc_flags(context, f, ccache, 
				   in_creds, NULL, NULL,
				   out_creds, ret_tgts);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_cred_from_kdc(krb5_context context,
		       krb5_ccache ccache,
		       krb5_creds *in_creds,
		       krb5_creds **out_creds,
		       krb5_creds ***ret_tgts)
{
    return krb5_get_cred_from_kdc_opt(context, ccache, 
				      in_creds, out_creds, ret_tgts, 0);
}
     

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_credentials_with_flags(krb5_context context,
				krb5_flags options,
				krb5_kdc_flags flags,
				krb5_ccache ccache,
				krb5_creds *in_creds,
				krb5_creds **out_creds)
{
    krb5_error_code ret;
    krb5_creds **tgts;
    krb5_creds *res_creds;
    int i;
    
    *out_creds = NULL;
    res_creds = calloc(1, sizeof(*res_creds));
    if (res_creds == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }

    if (in_creds->session.keytype)
	options |= KRB5_TC_MATCH_KEYTYPE;

    /* 
     * If we got a credential, check if credential is expired before
     * returning it.
     */
    ret = krb5_cc_retrieve_cred(context,
                                ccache,
                                in_creds->session.keytype ?
                                KRB5_TC_MATCH_KEYTYPE : 0,
                                in_creds, res_creds);
    /* 
     * If we got a credential, check if credential is expired before
     * returning it, but only if KRB5_GC_EXPIRED_OK is not set.
     */
    if (ret == 0) {
	krb5_timestamp timeret;

	/* If expired ok, don't bother checking */
        if(options & KRB5_GC_EXPIRED_OK) {
            *out_creds = res_creds;
            return 0;
        }
	    
	krb5_timeofday(context, &timeret);
	if(res_creds->times.endtime > timeret) {
	    *out_creds = res_creds;
	    return 0;
	}
	if(options & KRB5_GC_CACHED)
	    krb5_cc_remove_cred(context, ccache, 0, res_creds);

    } else if(ret != KRB5_CC_END) {
        free(res_creds);
        return ret;
    }
    free(res_creds);
    if(options & KRB5_GC_CACHED) {
	not_found(context, in_creds->server);
        return KRB5_CC_NOTFOUND;
    }
    if(options & KRB5_GC_USER_USER)
	flags.b.enc_tkt_in_skey = 1;
    if (flags.b.enc_tkt_in_skey)
	options |= KRB5_GC_NO_STORE;

    tgts = NULL;
    ret = get_cred_from_kdc_flags(context, flags, ccache, 
				  in_creds, NULL, NULL, out_creds, &tgts);
    for(i = 0; tgts && tgts[i]; i++) {
	krb5_cc_store_cred(context, ccache, tgts[i]);
	krb5_free_creds(context, tgts[i]);
    }
    free(tgts);
    if(ret == 0 && (options & KRB5_GC_NO_STORE) == 0)
	krb5_cc_store_cred(context, ccache, *out_creds);
    return ret;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_credentials(krb5_context context,
		     krb5_flags options,
		     krb5_ccache ccache,
		     krb5_creds *in_creds,
		     krb5_creds **out_creds)
{
    krb5_kdc_flags flags;
    flags.i = 0;
    return krb5_get_credentials_with_flags(context, options, flags,
					   ccache, in_creds, out_creds);
}

struct krb5_get_creds_opt_data {
    krb5_principal self;
    krb5_flags options;
    krb5_enctype enctype;
    Ticket *ticket;
};


krb5_error_code KRB5_LIB_FUNCTION
krb5_get_creds_opt_alloc(krb5_context context, krb5_get_creds_opt *opt)
{
    *opt = calloc(1, sizeof(**opt));
    if (*opt == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    return 0;
}

void KRB5_LIB_FUNCTION
krb5_get_creds_opt_free(krb5_context context, krb5_get_creds_opt opt)
{
    if (opt->self)
	krb5_free_principal(context, opt->self);
    memset(opt, 0, sizeof(*opt));
    free(opt);
}

void KRB5_LIB_FUNCTION
krb5_get_creds_opt_set_options(krb5_context context,
			       krb5_get_creds_opt opt,
			       krb5_flags options)
{
    opt->options = options;
}

void KRB5_LIB_FUNCTION
krb5_get_creds_opt_add_options(krb5_context context,
			       krb5_get_creds_opt opt,
			       krb5_flags options)
{
    opt->options |= options;
}

void KRB5_LIB_FUNCTION
krb5_get_creds_opt_set_enctype(krb5_context context,
			       krb5_get_creds_opt opt,
			       krb5_enctype enctype)
{
    opt->enctype = enctype;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_creds_opt_set_impersonate(krb5_context context,
				   krb5_get_creds_opt opt,
				   krb5_const_principal self)
{
    if (opt->self)
	krb5_free_principal(context, opt->self);
    return krb5_copy_principal(context, self, &opt->self);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_creds_opt_set_ticket(krb5_context context,
			      krb5_get_creds_opt opt,
			      const Ticket *ticket)
{
    if (opt->ticket) {
	free_Ticket(opt->ticket);
	free(opt->ticket);
	opt->ticket = NULL;
    }
    if (ticket) {
	krb5_error_code ret;

	opt->ticket = malloc(sizeof(*ticket));
	if (opt->ticket == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    return ENOMEM;
	}
	ret = copy_Ticket(ticket, opt->ticket);
	if (ret) {
	    free(opt->ticket);
	    opt->ticket = NULL;
	    krb5_set_error_string(context, "malloc: out of memory");
	    return ret;
	}
    }
    return 0;
}



krb5_error_code KRB5_LIB_FUNCTION
krb5_get_creds(krb5_context context,
	       krb5_get_creds_opt opt,
	       krb5_ccache ccache,
	       krb5_const_principal inprinc,
	       krb5_creds **out_creds)
{
    krb5_kdc_flags flags;
    krb5_flags options;
    krb5_creds in_creds;
    krb5_error_code ret;
    krb5_creds **tgts;
    krb5_creds *res_creds;
    int i;
    
    memset(&in_creds, 0, sizeof(in_creds));
    in_creds.server = rk_UNCONST(inprinc);

    ret = krb5_cc_get_principal(context, ccache, &in_creds.client);
    if (ret)
	return ret;

    options = opt->options;
    flags.i = 0;

    *out_creds = NULL;
    res_creds = calloc(1, sizeof(*res_creds));
    if (res_creds == NULL) {
	krb5_free_principal(context, in_creds.client);
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }

    if (opt->enctype) {
	in_creds.session.keytype = opt->enctype;
	options |= KRB5_TC_MATCH_KEYTYPE;
    }

    /* 
     * If we got a credential, check if credential is expired before
     * returning it.
     */
    ret = krb5_cc_retrieve_cred(context,
                                ccache,
				opt->enctype ? KRB5_TC_MATCH_KEYTYPE : 0,
                                &in_creds, res_creds);
    /* 
     * If we got a credential, check if credential is expired before
     * returning it, but only if KRB5_GC_EXPIRED_OK is not set.
     */
    if (ret == 0) {
	krb5_timestamp timeret;

	/* If expired ok, don't bother checking */
        if(options & KRB5_GC_EXPIRED_OK) {
            *out_creds = res_creds;
	    krb5_free_principal(context, in_creds.client);
            return 0;
        }
	    
	krb5_timeofday(context, &timeret);
	if(res_creds->times.endtime > timeret) {
	    *out_creds = res_creds;
	    krb5_free_principal(context, in_creds.client);
	    return 0;
	}
	if(options & KRB5_GC_CACHED)
	    krb5_cc_remove_cred(context, ccache, 0, res_creds);

    } else if(ret != KRB5_CC_END) {
        free(res_creds);
	krb5_free_principal(context, in_creds.client);
        return ret;
    }
    free(res_creds);
    if(options & KRB5_GC_CACHED) {
	not_found(context, in_creds.server);
	krb5_free_principal(context, in_creds.client);
        return KRB5_CC_NOTFOUND;
    }
    if(options & KRB5_GC_USER_USER) {
	flags.b.enc_tkt_in_skey = 1;
	options |= KRB5_GC_NO_STORE;
    }
    if (options & KRB5_GC_FORWARDABLE)
	flags.b.forwardable = 1;
    if (options & KRB5_GC_NO_TRANSIT_CHECK)
	flags.b.disable_transited_check = 1;
    if (options & KRB5_GC_CONSTRAINED_DELEGATION) {
	flags.b.request_anonymous = 1; /* XXX ARGH confusion */
	flags.b.constrained_delegation = 1;
    }

    tgts = NULL;
    ret = get_cred_from_kdc_flags(context, flags, ccache, 
				  &in_creds, opt->self, opt->ticket,
				  out_creds, &tgts);
    krb5_free_principal(context, in_creds.client);
    for(i = 0; tgts && tgts[i]; i++) {
	krb5_cc_store_cred(context, ccache, tgts[i]);
	krb5_free_creds(context, tgts[i]);
    }
    free(tgts);
    if(ret == 0 && (options & KRB5_GC_NO_STORE) == 0)
	krb5_cc_store_cred(context, ccache, *out_creds);
    return ret;
}

/*
 *
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_renewed_creds(krb5_context context,
		       krb5_creds *creds,
		       krb5_const_principal client,
		       krb5_ccache ccache,
		       const char *in_tkt_service)
{
    krb5_error_code ret;
    krb5_kdc_flags flags;
    krb5_creds in, *template, *out = NULL;

    memset(&in, 0, sizeof(in));
    memset(creds, 0, sizeof(*creds));

    ret = krb5_copy_principal(context, client, &in.client);
    if (ret)
	return ret;

    if (in_tkt_service) {
	ret = krb5_parse_name(context, in_tkt_service, &in.server);
	if (ret) {
	    krb5_free_principal(context, in.client);
	    return ret;
	}
    } else {
	const char *realm = krb5_principal_get_realm(context, client);
	
	ret = krb5_make_principal(context, &in.server, realm, KRB5_TGS_NAME,
				  realm, NULL);
	if (ret) {
	    krb5_free_principal(context, in.client);
	    return ret;
	}
    }

    flags.i = 0;
    flags.b.renewable = flags.b.renew = 1;

    /*
     * Get template from old credential cache for the same entry, if
     * this failes, no worries.
     */
    ret = krb5_get_credentials(context, KRB5_GC_CACHED, ccache, &in, &template);
    if (ret == 0) {
	flags.b.forwardable = template->flags.b.forwardable;
	flags.b.proxiable = template->flags.b.proxiable;
	krb5_free_creds (context, template);
    }

    ret = krb5_get_kdc_cred(context, ccache, flags, NULL, NULL, &in, &out);
    krb5_free_principal(context, in.client);
    krb5_free_principal(context, in.server);
    if (ret)
	return ret;

    ret = krb5_copy_creds_contents(context, out, creds);
    krb5_free_creds(context, out);

    return ret;
}
