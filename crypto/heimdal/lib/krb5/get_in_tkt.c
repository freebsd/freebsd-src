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

#include "krb5_locl.h"

RCSID("$Id: get_in_tkt.c,v 1.100 2001/05/14 06:14:48 assar Exp $");

krb5_error_code
krb5_init_etype (krb5_context context,
		 unsigned *len,
		 int **val,
		 const krb5_enctype *etypes)
{
    int i;
    krb5_error_code ret;
    krb5_enctype *tmp;

    ret = 0;
    if (etypes)
	tmp = (krb5_enctype*)etypes;
    else {
	ret = krb5_get_default_in_tkt_etypes(context,
					     &tmp);
	if (ret)
	    return ret;
    }

    for (i = 0; tmp[i]; ++i)
	;
    *len = i;
    *val = malloc(i * sizeof(int));
    if (i != 0 && *val == NULL) {
	ret = ENOMEM;
	krb5_set_error_string(context, "malloc: out of memory");
	goto cleanup;
    }
    memmove (*val,
	     tmp,
	     i * sizeof(*tmp));
cleanup:
    if (etypes == NULL)
	free (tmp);
    return ret;
}


static krb5_error_code
decrypt_tkt (krb5_context context,
	     krb5_keyblock *key,
	     krb5_key_usage usage,
	     krb5_const_pointer decrypt_arg,
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
    if (ret)
	return ret;
    return 0;
}

int
_krb5_extract_ticket(krb5_context context, 
		     krb5_kdc_rep *rep, 
		     krb5_creds *creds,		
		     krb5_keyblock *key,
		     krb5_const_pointer keyseed,
		     krb5_key_usage key_usage,
		     krb5_addresses *addrs,
		     unsigned nonce,
		     krb5_boolean allow_server_mismatch,
		     krb5_boolean ignore_cname,
		     krb5_decrypt_proc decrypt_proc,
		     krb5_const_pointer decryptarg)
{
    krb5_error_code ret;
    krb5_principal tmp_principal;
    int tmp;
    time_t tmp_time;
    krb5_timestamp sec_now;

    ret = principalname2krb5_principal (&tmp_principal,
					rep->kdc_rep.cname,
					rep->kdc_rep.crealm);
    if (ret)
	goto out;

    /* compare client */

    if (!ignore_cname) {
	tmp = krb5_principal_compare (context, tmp_principal, creds->client);
	if (!tmp) {
	    krb5_free_principal (context, tmp_principal);
	    krb5_clear_error_string (context);
	    ret = KRB5KRB_AP_ERR_MODIFIED;
	    goto out;
	}
    }

    krb5_free_principal (context, creds->client);
    creds->client = tmp_principal;

    /* extract ticket */
    {
	unsigned char *buf;
	size_t len;
	len = length_Ticket(&rep->kdc_rep.ticket);
	buf = malloc(len);
	if(buf == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    ret = ENOMEM;
	    goto out;
	}
	encode_Ticket(buf + len - 1, len, &rep->kdc_rep.ticket, &len);
	creds->ticket.data = buf;
	creds->ticket.length = len;
	creds->second_ticket.length = 0;
	creds->second_ticket.data   = NULL;
    }

    /* compare server */

    ret = principalname2krb5_principal (&tmp_principal,
					rep->kdc_rep.ticket.sname,
					rep->kdc_rep.ticket.realm);
    if (ret)
	goto out;
    if(allow_server_mismatch){
	krb5_free_principal(context, creds->server);
	creds->server = tmp_principal;
	tmp_principal = NULL;
    }else{
	tmp = krb5_principal_compare (context, tmp_principal, creds->server);
	krb5_free_principal (context, tmp_principal);
	if (!tmp) {
	    ret = KRB5KRB_AP_ERR_MODIFIED;
	    krb5_clear_error_string (context);
	    goto out;
	}
    }
    
    /* decrypt */

    if (decrypt_proc == NULL)
	decrypt_proc = decrypt_tkt;
    
    ret = (*decrypt_proc)(context, key, key_usage, decryptarg, rep);
    if (ret)
	goto out;

#if 0
    /* XXX should this decode be here, or in the decrypt_proc? */
    ret = krb5_decode_keyblock(context, &rep->enc_part.key, 1);
    if(ret)
	goto out;
#endif

    /* compare nonces */

    if (nonce != rep->enc_part.nonce) {
	ret = KRB5KRB_AP_ERR_MODIFIED;
	krb5_set_error_string(context, "malloc: out of memory");
	goto out;
    }

    /* set kdc-offset */

    krb5_timeofday (context, &sec_now);
    if (context->kdc_sec_offset == 0
	&& krb5_config_get_bool (context, NULL,
				 "libdefaults",
				 "kdc_timesync",
				 NULL)) {
	context->kdc_sec_offset = rep->enc_part.authtime - sec_now;
	krb5_timeofday (context, &sec_now);
    }

    /* check all times */

    if (rep->enc_part.starttime) {
	tmp_time = *rep->enc_part.starttime;
    } else
	tmp_time = rep->enc_part.authtime;

    if (creds->times.starttime == 0
	&& abs(tmp_time - sec_now) > context->max_skew) {
	ret = KRB5KRB_AP_ERR_SKEW;
	krb5_set_error_string (context,
			       "time skew (%d) larger than max (%d)",
			       abs(tmp_time - sec_now),
			       (int)context->max_skew);
	goto out;
    }

    if (creds->times.starttime != 0
	&& tmp_time != creds->times.starttime) {
	krb5_clear_error_string (context);
	ret = KRB5KRB_AP_ERR_MODIFIED;
	goto out;
    }

    creds->times.starttime = tmp_time;

    if (rep->enc_part.renew_till) {
	tmp_time = *rep->enc_part.renew_till;
    } else
	tmp_time = 0;

    if (creds->times.renew_till != 0
	&& tmp_time > creds->times.renew_till) {
	krb5_clear_error_string (context);
	ret = KRB5KRB_AP_ERR_MODIFIED;
	goto out;
    }

    creds->times.renew_till = tmp_time;

    creds->times.authtime = rep->enc_part.authtime;

    if (creds->times.endtime != 0
	&& rep->enc_part.endtime > creds->times.endtime) {
	krb5_clear_error_string (context);
	ret = KRB5KRB_AP_ERR_MODIFIED;
	goto out;
    }

    creds->times.endtime  = rep->enc_part.endtime;

    if(rep->enc_part.caddr)
	krb5_copy_addresses (context, rep->enc_part.caddr, &creds->addresses);
    else if(addrs)
	krb5_copy_addresses (context, addrs, &creds->addresses);
    else {
	creds->addresses.len = 0;
	creds->addresses.val = NULL;
    }
    creds->flags.b = rep->enc_part.flags;
	  
    creds->authdata.len = 0;
    creds->authdata.val = NULL;
    creds->session.keyvalue.length = 0;
    creds->session.keyvalue.data   = NULL;
    creds->session.keytype = rep->enc_part.key.keytype;
    ret = krb5_data_copy (&creds->session.keyvalue,
			  rep->enc_part.key.keyvalue.data,
			  rep->enc_part.key.keyvalue.length);

out:
    memset (rep->enc_part.key.keyvalue.data, 0,
	    rep->enc_part.key.keyvalue.length);
    return ret;
}


static krb5_error_code
make_pa_enc_timestamp(krb5_context context, PA_DATA *pa, 
		      krb5_enctype etype, krb5_keyblock *key)
{
    PA_ENC_TS_ENC p;
    u_char buf[1024];
    size_t len;
    EncryptedData encdata;
    krb5_error_code ret;
    int32_t sec, usec;
    int usec2;
    krb5_crypto crypto;
    
    krb5_us_timeofday (context, &sec, &usec);
    p.patimestamp = sec;
    usec2         = usec;
    p.pausec      = &usec2;

    ret = encode_PA_ENC_TS_ENC(buf + sizeof(buf) - 1,
			       sizeof(buf),
			       &p,
			       &len);
    if (ret)
	return ret;

    ret = krb5_crypto_init(context, key, 0, &crypto);
    if (ret)
	return ret;
    ret = krb5_encrypt_EncryptedData(context, 
				     crypto,
				     KRB5_KU_PA_ENC_TIMESTAMP,
				     buf + sizeof(buf) - len,
				     len,
				     0,
				     &encdata);
    krb5_crypto_destroy(context, crypto);
    if (ret)
	return ret;
		    
    ret = encode_EncryptedData(buf + sizeof(buf) - 1,
			       sizeof(buf),
			       &encdata, 
			       &len);
    free_EncryptedData(&encdata);
    if (ret)
	return ret;
    pa->padata_type = KRB5_PADATA_ENC_TIMESTAMP;
    pa->padata_value.length = 0;
    krb5_data_copy(&pa->padata_value,
		   buf + sizeof(buf) - len,
		   len);
    return 0;
}

static krb5_error_code
add_padata(krb5_context context,
	   METHOD_DATA *md, 
	   krb5_principal client,
	   krb5_key_proc key_proc,
	   krb5_const_pointer keyseed,
	   int *enctypes, 
	   unsigned netypes,
	   krb5_salt *salt)
{
    krb5_error_code ret;
    PA_DATA *pa2;
    krb5_salt salt2;
    int *ep;
    int i;
    
    if(salt == NULL) {
	/* default to standard salt */
	ret = krb5_get_pw_salt (context, client, &salt2);
	salt = &salt2;
    }
    if (!enctypes) {
	enctypes = (int *)context->etypes; /* XXX */
	netypes = 0;
	for (ep = enctypes; *ep != ETYPE_NULL; ep++)
	    netypes++;
    }
    pa2 = realloc (md->val, (md->len + netypes) * sizeof(*md->val));
    if (pa2 == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    md->val = pa2;

    for (i = 0; i < netypes; ++i) {
	krb5_keyblock *key;

	ret = (*key_proc)(context, enctypes[i], *salt, keyseed, &key);
	if (ret)
	    continue;
	ret = make_pa_enc_timestamp (context, &md->val[md->len],
				     enctypes[i], key);
	krb5_free_keyblock (context, key);
	if (ret)
	    return ret;
	++md->len;
    }
    if(salt == &salt2)
	krb5_free_salt(context, salt2);
    return 0;
}

static krb5_error_code
init_as_req (krb5_context context,
	     krb5_kdc_flags opts,
	     krb5_creds *creds,
	     const krb5_addresses *addrs,
	     const krb5_enctype *etypes,
	     const krb5_preauthtype *ptypes,
	     const krb5_preauthdata *preauth,
	     krb5_key_proc key_proc,
	     krb5_const_pointer keyseed,
	     unsigned nonce,
	     AS_REQ *a)
{
    krb5_error_code ret;
    krb5_salt salt;

    memset(a, 0, sizeof(*a));

    a->pvno = 5;
    a->msg_type = krb_as_req;
    a->req_body.kdc_options = opts.b;
    a->req_body.cname = malloc(sizeof(*a->req_body.cname));
    if (a->req_body.cname == NULL) {
	ret = ENOMEM;
	krb5_set_error_string(context, "malloc: out of memory");
	goto fail;
    }
    a->req_body.sname = malloc(sizeof(*a->req_body.sname));
    if (a->req_body.sname == NULL) {
	ret = ENOMEM;
	krb5_set_error_string(context, "malloc: out of memory");
	goto fail;
    }
    ret = krb5_principal2principalname (a->req_body.cname, creds->client);
    if (ret)
	goto fail;
    ret = krb5_principal2principalname (a->req_body.sname, creds->server);
    if (ret)
	goto fail;
    ret = copy_Realm(&creds->client->realm, &a->req_body.realm);
    if (ret)
	goto fail;

    if(creds->times.starttime) {
	a->req_body.from = malloc(sizeof(*a->req_body.from));
	if (a->req_body.from == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_string(context, "malloc: out of memory");
	    goto fail;
	}
	*a->req_body.from = creds->times.starttime;
    }
    if(creds->times.endtime){
	ALLOC(a->req_body.till, 1);
	*a->req_body.till = creds->times.endtime;
    }
    if(creds->times.renew_till){
	a->req_body.rtime = malloc(sizeof(*a->req_body.rtime));
	if (a->req_body.rtime == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_string(context, "malloc: out of memory");
	    goto fail;
	}
	*a->req_body.rtime = creds->times.renew_till;
    }
    a->req_body.nonce = nonce;
    ret = krb5_init_etype (context,
			   &a->req_body.etype.len,
			   &a->req_body.etype.val,
			   etypes);
    if (ret)
	goto fail;

    /*
     * This means no addresses
     */

    if (addrs && addrs->len == 0) {
	a->req_body.addresses = NULL;
    } else {
	a->req_body.addresses = malloc(sizeof(*a->req_body.addresses));
	if (a->req_body.addresses == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_string(context, "malloc: out of memory");
	    goto fail;
	}

	if (addrs)
	    ret = krb5_copy_addresses(context, addrs, a->req_body.addresses);
	else
	    ret = krb5_get_all_client_addrs (context, a->req_body.addresses);
	if (ret)
	    return ret;
    }

    a->req_body.enc_authorization_data = NULL;
    a->req_body.additional_tickets = NULL;

    if(preauth != NULL) {
	int i;
	ALLOC(a->padata, 1);
	if(a->padata == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_string(context, "malloc: out of memory");
	    goto fail;
	}
	for(i = 0; i < preauth->len; i++) {
	    if(preauth->val[i].type == KRB5_PADATA_ENC_TIMESTAMP){
		int j;
		PA_DATA *tmp = realloc(a->padata->val, 
				       (a->padata->len + 
					preauth->val[i].info.len) * 
				       sizeof(*a->padata->val));
		if(tmp == NULL) {
		    ret = ENOMEM;
		    krb5_set_error_string(context, "malloc: out of memory");
		    goto fail;
		}
		a->padata->val = tmp;
		for(j = 0; j < preauth->val[i].info.len; j++) {
		    krb5_salt *sp = &salt;
		    if(preauth->val[i].info.val[j].salttype)
			salt.salttype = *preauth->val[i].info.val[j].salttype;
		    else
			salt.salttype = KRB5_PW_SALT;
		    if(preauth->val[i].info.val[j].salt)
			salt.saltvalue = *preauth->val[i].info.val[j].salt;
		    else
			if(salt.salttype == KRB5_PW_SALT)
			    sp = NULL;
			else
			    krb5_data_zero(&salt.saltvalue);
		    add_padata(context, a->padata, creds->client, 
			       key_proc, keyseed, 
			       &preauth->val[i].info.val[j].etype, 1,
			       sp);
		}
	    }
	}
    } else 
    /* not sure this is the way to use `ptypes' */
    if (ptypes == NULL || *ptypes == KRB5_PADATA_NONE)
	a->padata = NULL;
    else if (*ptypes ==  KRB5_PADATA_ENC_TIMESTAMP) {
	ALLOC(a->padata, 1);
	if (a->padata == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_string(context, "malloc: out of memory");
	    goto fail;
	}
	a->padata->len = 0;
	a->padata->val = NULL;

	/* make a v5 salted pa-data */
	add_padata(context, a->padata, creds->client, 
		   key_proc, keyseed, a->req_body.etype.val,
		   a->req_body.etype.len, NULL);
	
	/* make a v4 salted pa-data */
	salt.salttype = KRB5_PW_SALT;
	krb5_data_zero(&salt.saltvalue);
	add_padata(context, a->padata, creds->client, 
		   key_proc, keyseed, a->req_body.etype.val,
		   a->req_body.etype.len, &salt);
    } else {
	krb5_set_error_string (context, "pre-auth type %d not supported",
			       *ptypes);
	ret = KRB5_PREAUTH_BAD_TYPE;
	goto fail;
    }
    return 0;
fail:
    free_AS_REQ(a);
    return ret;
}

static int
set_ptypes(krb5_context context,
	   KRB_ERROR *error, 
	   krb5_preauthtype **ptypes,
	   krb5_preauthdata **preauth)
{
    static krb5_preauthdata preauth2;
    static krb5_preauthtype ptypes2[] = { KRB5_PADATA_ENC_TIMESTAMP, KRB5_PADATA_NONE };

    if(error->e_data) {
	METHOD_DATA md;
	int i;
	decode_METHOD_DATA(error->e_data->data, 
			   error->e_data->length, 
			   &md, 
			   NULL);
	for(i = 0; i < md.len; i++){
	    switch(md.val[i].padata_type){
	    case KRB5_PADATA_ENC_TIMESTAMP:
		*ptypes = ptypes2;
		break;
	    case KRB5_PADATA_ETYPE_INFO:
		*preauth = &preauth2;
		ALLOC_SEQ(*preauth, 1);
		(*preauth)->val[0].type = KRB5_PADATA_ENC_TIMESTAMP;
		krb5_decode_ETYPE_INFO(context,
				       md.val[i].padata_value.data, 
				       md.val[i].padata_value.length,
				       &(*preauth)->val[0].info,
				       NULL);
		break;
	    default:
		break;
	    }
	}
	free_METHOD_DATA(&md);
    } else {
	*ptypes = ptypes2;
    }
    return(1);
}

krb5_error_code
krb5_get_in_cred(krb5_context context,
		 krb5_flags options,
		 const krb5_addresses *addrs,
		 const krb5_enctype *etypes,
		 const krb5_preauthtype *ptypes,
		 const krb5_preauthdata *preauth,
		 krb5_key_proc key_proc,
		 krb5_const_pointer keyseed,
		 krb5_decrypt_proc decrypt_proc,
		 krb5_const_pointer decryptarg,
		 krb5_creds *creds,
		 krb5_kdc_rep *ret_as_reply)
{
    krb5_error_code ret;
    AS_REQ a;
    krb5_kdc_rep rep;
    krb5_data req, resp;
    char buf[BUFSIZ];
    krb5_salt salt;
    krb5_keyblock *key;
    size_t size;
    krb5_kdc_flags opts;
    PA_DATA *pa;
    krb5_enctype etype;
    krb5_preauthdata *my_preauth = NULL;
    unsigned nonce;
    int done;

    opts.i = options;

    krb5_generate_random_block (&nonce, sizeof(nonce));
    nonce &= 0xffffffff;

    do {
	done = 1;
	ret = init_as_req (context,
			   opts,
			   creds,
			   addrs,
			   etypes,
			   ptypes,
			   preauth,
			   key_proc,
			   keyseed,
			   nonce,
			   &a);
	if (my_preauth) {
	    free_ETYPE_INFO(&my_preauth->val[0].info);
	    free (my_preauth->val);
	}
	if (ret)
	    return ret;

	ret = encode_AS_REQ ((unsigned char*)buf + sizeof(buf) - 1,
			     sizeof(buf),
			     &a,
			     &req.length);
	free_AS_REQ(&a);
	if (ret)
	    return ret;

	req.data = buf + sizeof(buf) - req.length;

	ret = krb5_sendto_kdc (context, &req, &creds->client->realm, &resp);
	if (ret)
	    return ret;

	memset (&rep, 0, sizeof(rep));
	ret = decode_AS_REP(resp.data, resp.length, &rep.kdc_rep, &size);
	if(ret) {
	    /* let's try to parse it as a KRB-ERROR */
	    KRB_ERROR error;
	    int ret2;

	    ret2 = krb5_rd_error(context, &resp, &error);
	    if(ret2 && resp.data && ((char*)resp.data)[0] == 4)
		ret = KRB5KRB_AP_ERR_V4_REPLY;
	    krb5_data_free(&resp);
	    if (ret2 == 0) {
		ret = krb5_error_from_rd_error(context, &error, creds);
		/* if no preauth was set and KDC requires it, give it
                   one more try */
		if (!ptypes && !preauth
		    && ret == KRB5KDC_ERR_PREAUTH_REQUIRED
#if 0
			|| ret == KRB5KDC_ERR_BADOPTION
#endif
		    && set_ptypes(context, &error, &ptypes, &my_preauth)) {
		    done = 0;
		    preauth = my_preauth;
		    krb5_free_error_contents(context, &error);
		    continue;
		}
		if(ret_as_reply)
		    ret_as_reply->error = error;
		else
		    free_KRB_ERROR (&error);
		return ret;
	    }
	    return ret;
	}
	krb5_data_free(&resp);
    } while(!done);
    
    pa = NULL;
    etype = rep.kdc_rep.enc_part.etype;
    if(rep.kdc_rep.padata){
	int index = 0;
	pa = krb5_find_padata(rep.kdc_rep.padata->val, rep.kdc_rep.padata->len, 
			      KRB5_PADATA_PW_SALT, &index);
	if(pa == NULL) {
	    index = 0;
	    pa = krb5_find_padata(rep.kdc_rep.padata->val, 
				  rep.kdc_rep.padata->len, 
				  KRB5_PADATA_AFS3_SALT, &index);
	}
    }
    if(pa) {
	salt.salttype = pa->padata_type;
	salt.saltvalue = pa->padata_value;
	
	ret = (*key_proc)(context, etype, salt, keyseed, &key);
    } else {
	/* make a v5 salted pa-data */
	ret = krb5_get_pw_salt (context, creds->client, &salt);
	
	if (ret)
	    goto out;
	ret = (*key_proc)(context, etype, salt, keyseed, &key);
	krb5_free_salt(context, salt);
    }
    if (ret)
	goto out;
	
    ret = _krb5_extract_ticket(context, 
			       &rep, 
			       creds, 
			       key, 
			       keyseed, 
			       KRB5_KU_AS_REP_ENC_PART,
			       NULL, 
			       nonce, 
			       FALSE, 
			       opts.b.request_anonymous,
			       decrypt_proc, 
			       decryptarg);
    memset (key->keyvalue.data, 0, key->keyvalue.length);
    krb5_free_keyblock_contents (context, key);
    free (key);

out:
    if (ret == 0 && ret_as_reply)
	*ret_as_reply = rep;
    else
	krb5_free_kdc_rep (context, &rep);
    return ret;
}

krb5_error_code
krb5_get_in_tkt(krb5_context context,
		krb5_flags options,
		const krb5_addresses *addrs,
		const krb5_enctype *etypes,
		const krb5_preauthtype *ptypes,
		krb5_key_proc key_proc,
		krb5_const_pointer keyseed,
		krb5_decrypt_proc decrypt_proc,
		krb5_const_pointer decryptarg,
		krb5_creds *creds,
		krb5_ccache ccache,
		krb5_kdc_rep *ret_as_reply)
{
    krb5_error_code ret;
    krb5_kdc_flags opts;
    opts.i = 0;
    opts.b = int2KDCOptions(options);
    
    ret = krb5_get_in_cred (context,
			    opts.i,
			    addrs,
			    etypes,
			    ptypes,
			    NULL,
			    key_proc,
			    keyseed,
			    decrypt_proc,
			    decryptarg,
			    creds,
			    ret_as_reply);
    if(ret) 
	return ret;
    ret = krb5_cc_store_cred (context, ccache, creds);
    krb5_free_creds_contents (context, creds);
    return ret;
}
