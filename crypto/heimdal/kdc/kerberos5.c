/*
 * Copyright (c) 1997-2002 Kungliga Tekniska Högskolan
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

#include "kdc_locl.h"

RCSID("$Id: kerberos5.c,v 1.143 2002/09/09 14:03:02 nectar Exp $");

#define MAX_TIME ((time_t)((1U << 31) - 1))

static void
fix_time(time_t **t)
{
    if(*t == NULL){
	ALLOC(*t);
	**t = MAX_TIME;
    }
    if(**t == 0) **t = MAX_TIME; /* fix for old clients */
}

static void
set_salt_padata (METHOD_DATA **m, Salt *salt)
{
    if (salt) {
	ALLOC(*m);
	(*m)->len = 1;
	ALLOC((*m)->val);
	(*m)->val->padata_type = salt->type;
	copy_octet_string(&salt->salt,
			  &(*m)->val->padata_value);
    }
}

static PA_DATA*
find_padata(KDC_REQ *req, int *start, int type)
{
    while(*start < req->padata->len){
	(*start)++;
	if(req->padata->val[*start - 1].padata_type == type)
	    return &req->padata->val[*start - 1];
    }
    return NULL;
}

/*
 * return the first appropriate key of `princ' in `ret_key'.  Look for
 * all the etypes in (`etypes', `len'), stopping as soon as we find
 * one, but preferring one that has default salt
 */

static krb5_error_code
find_etype(hdb_entry *princ, krb5_enctype *etypes, unsigned len, 
	   Key **ret_key, krb5_enctype *ret_etype)
{
    int i;
    krb5_error_code ret = KRB5KDC_ERR_ETYPE_NOSUPP;

    for(i = 0; ret != 0 && i < len ; i++) {
	Key *key = NULL;

	while (hdb_next_enctype2key(context, princ, etypes[i], &key) == 0) {
	    if (key->key.keyvalue.length == 0) {
		ret = KRB5KDC_ERR_NULL_KEY;
		continue;
	    }
	    *ret_key   = key;
	    *ret_etype = etypes[i];
	    ret = 0;
	    if (key->salt == NULL)
		return ret;
	}
    }
    return ret;
}

static krb5_error_code
find_keys(hdb_entry *client,
	  hdb_entry *server, 
	  Key **ckey,
	  krb5_enctype *cetype,
	  Key **skey,
	  krb5_enctype *setype, 
	  krb5_enctype *etypes,
	  unsigned num_etypes)
{
    krb5_error_code ret;

    if(client){
	/* find client key */
	ret = find_etype(client, etypes, num_etypes, ckey, cetype);
	if (ret) {
	    kdc_log(0, "Client has no support for etypes");
	    return ret;
	}
    }

    if(server){
	/* find server key */
	ret = find_etype(server, etypes, num_etypes, skey, setype);
	if (ret) {
	    kdc_log(0, "Server has no support for etypes");
	    return ret;
	}
    }
    return 0;
}

static krb5_error_code
make_anonymous_principalname (PrincipalName *pn)
{
    pn->name_type = KRB5_NT_PRINCIPAL;
    pn->name_string.len = 1;
    pn->name_string.val = malloc(sizeof(*pn->name_string.val));
    if (pn->name_string.val == NULL)
	return ENOMEM;
    pn->name_string.val[0] = strdup("anonymous");
    if (pn->name_string.val[0] == NULL) {
	free(pn->name_string.val);
	pn->name_string.val = NULL;
	return ENOMEM;
    }
    return 0;
}

static krb5_error_code
encode_reply(KDC_REP *rep, EncTicketPart *et, EncKDCRepPart *ek, 
	     krb5_enctype etype, 
	     int skvno, EncryptionKey *skey,
	     int ckvno, EncryptionKey *ckey,
	     const char **e_text,
	     krb5_data *reply)
{
    unsigned char *buf;
    size_t buf_size;
    size_t len;
    krb5_error_code ret;
    krb5_crypto crypto;

    ASN1_MALLOC_ENCODE(EncTicketPart, buf, buf_size, et, &len, ret);
    if(ret) {
	kdc_log(0, "Failed to encode ticket: %s", 
		krb5_get_err_text(context, ret));
	return ret;
    }
    if(buf_size != len) {
	free(buf);
	kdc_log(0, "Internal error in ASN.1 encoder");
	*e_text = "KDC internal error";
	return KRB5KRB_ERR_GENERIC;
    }

    ret = krb5_crypto_init(context, skey, etype, &crypto);
    if (ret) {
	free(buf);
	kdc_log(0, "krb5_crypto_init failed: %s",
		krb5_get_err_text(context, ret));
	return ret;
    }

    ret = krb5_encrypt_EncryptedData(context, 
				     crypto,
				     KRB5_KU_TICKET,
				     buf,
				     len,
				     skvno,
				     &rep->ticket.enc_part);
    free(buf);
    krb5_crypto_destroy(context, crypto);
    if(ret) {
	kdc_log(0, "Failed to encrypt data: %s",
		krb5_get_err_text(context, ret));
	return ret;
    }
    
    if(rep->msg_type == krb_as_rep && !encode_as_rep_as_tgs_rep)
	ASN1_MALLOC_ENCODE(EncASRepPart, buf, buf_size, ek, &len, ret);
    else
	ASN1_MALLOC_ENCODE(EncTGSRepPart, buf, buf_size, ek, &len, ret);
    if(ret) {
	kdc_log(0, "Failed to encode KDC-REP: %s", 
		krb5_get_err_text(context, ret));
	return ret;
    }
    if(buf_size != len) {
	free(buf);
	kdc_log(0, "Internal error in ASN.1 encoder");
	*e_text = "KDC internal error";
	return KRB5KRB_ERR_GENERIC;
    }
    ret = krb5_crypto_init(context, ckey, 0, &crypto);
    if (ret) {
	free(buf);
	kdc_log(0, "krb5_crypto_init failed: %s",
		krb5_get_err_text(context, ret));
	return ret;
    }
    if(rep->msg_type == krb_as_rep) {
	krb5_encrypt_EncryptedData(context,
				   crypto,
				   KRB5_KU_AS_REP_ENC_PART,
				   buf,
				   len,
				   ckvno,
				   &rep->enc_part);
	free(buf);
	ASN1_MALLOC_ENCODE(AS_REP, buf, buf_size, rep, &len, ret);
    } else {
	krb5_encrypt_EncryptedData(context,
				   crypto,
				   KRB5_KU_TGS_REP_ENC_PART_SESSION,
				   buf,
				   len,
				   ckvno,
				   &rep->enc_part);
	free(buf);
	ASN1_MALLOC_ENCODE(TGS_REP, buf, buf_size, rep, &len, ret);
    }
    krb5_crypto_destroy(context, crypto);
    if(ret) {
	kdc_log(0, "Failed to encode KDC-REP: %s", 
		krb5_get_err_text(context, ret));
	return ret;
    }
    if(buf_size != len) {
	free(buf);
	kdc_log(0, "Internal error in ASN.1 encoder");
	*e_text = "KDC internal error";
	return KRB5KRB_ERR_GENERIC;
    }
    reply->data = buf;
    reply->length = buf_size;
    return 0;
}

static int
realloc_method_data(METHOD_DATA *md)
{
    PA_DATA *pa;
    pa = realloc(md->val, (md->len + 1) * sizeof(*md->val));
    if(pa == NULL)
	return ENOMEM;
    md->val = pa;
    md->len++;
    return 0;
}

static krb5_error_code
make_etype_info_entry(ETYPE_INFO_ENTRY *ent, Key *key)
{
    ent->etype = key->key.keytype;
    if(key->salt){
	ALLOC(ent->salttype);
#if 0
	if(key->salt->type == hdb_pw_salt)
	    *ent->salttype = 0; /* or 1? or NULL? */
	else if(key->salt->type == hdb_afs3_salt)
	    *ent->salttype = 2;
	else {
	    kdc_log(0, "unknown salt-type: %d", 
		    key->salt->type);
	    return KRB5KRB_ERR_GENERIC;
	}
	/* according to `the specs', we can't send a salt if
	   we have AFS3 salted key, but that requires that you
	   *know* what cell you are using (e.g by assuming
	   that the cell is the same as the realm in lower
	   case) */
#else
	*ent->salttype = key->salt->type;
#endif
	krb5_copy_data(context, &key->salt->salt,
		       &ent->salt);
    } else {
	/* we return no salt type at all, as that should indicate
	 * the default salt type and make everybody happy.  some
	 * systems (like w2k) dislike being told the salt type
	 * here. */

	ent->salttype = NULL;
	ent->salt = NULL;
    }
    return 0;
}

static krb5_error_code
get_pa_etype_info(METHOD_DATA *md, hdb_entry *client, 
		  ENCTYPE *etypes, unsigned int etypes_len)
{
    krb5_error_code ret = 0;
    int i, j;
    unsigned int n = 0;
    ETYPE_INFO pa;
    unsigned char *buf;
    size_t len;
    

    pa.len = client->keys.len;
    if(pa.len > UINT_MAX/sizeof(*pa.val))
	return ERANGE;
    pa.val = malloc(pa.len * sizeof(*pa.val));
    if(pa.val == NULL)
	return ENOMEM;

    for(j = 0; j < etypes_len; j++) {
	for(i = 0; i < client->keys.len; i++) {
	    if(client->keys.val[i].key.keytype == etypes[j])
		if((ret = make_etype_info_entry(&pa.val[n++], 
						&client->keys.val[i])) != 0) {
		    free_ETYPE_INFO(&pa);
		    return ret;
		}
	}
    }
    for(i = 0; i < client->keys.len; i++) {
	for(j = 0; j < etypes_len; j++) {
	    if(client->keys.val[i].key.keytype == etypes[j])
		goto skip;
	}
	if((ret = make_etype_info_entry(&pa.val[n++], 
					&client->keys.val[i])) != 0) {
	    free_ETYPE_INFO(&pa);
	    return ret;
	}
      skip:;
    }
    
    if(n != pa.len) {
	char *name;
	ret = krb5_unparse_name(context, client->principal, &name);
	if (ret)
	    name = "<unparse_name failed>";
	kdc_log(0, "internal error in get_pa_etype_info(%s): %d != %d", 
		name, n, pa.len);
	if (ret == 0)
	    free(name);
	pa.len = n;
    }

    ASN1_MALLOC_ENCODE(ETYPE_INFO, buf, len, &pa, &len, ret);
    free_ETYPE_INFO(&pa);
    if(ret)
	return ret;
    ret = realloc_method_data(md);
    if(ret) {
	free(buf);
	return ret;
    }
    md->val[md->len - 1].padata_type = KRB5_PADATA_ETYPE_INFO;
    md->val[md->len - 1].padata_value.length = len;
    md->val[md->len - 1].padata_value.data = buf;
    return 0;
}

/*
 * verify the flags on `client' and `server', returning 0
 * if they are OK and generating an error messages and returning
 * and error code otherwise.
 */

krb5_error_code
check_flags(hdb_entry *client, const char *client_name,
	    hdb_entry *server, const char *server_name,
	    krb5_boolean is_as_req)
{
    if(client != NULL) {
	/* check client */
	if (client->flags.invalid) {
	    kdc_log(0, "Client (%s) has invalid bit set", client_name);
	    return KRB5KDC_ERR_POLICY;
	}
	
	if(!client->flags.client){
	    kdc_log(0, "Principal may not act as client -- %s", 
		    client_name);
	    return KRB5KDC_ERR_POLICY;
	}
	
	if (client->valid_start && *client->valid_start > kdc_time) {
	    kdc_log(0, "Client not yet valid -- %s", client_name);
	    return KRB5KDC_ERR_CLIENT_NOTYET;
	}
	
	if (client->valid_end && *client->valid_end < kdc_time) {
	    kdc_log(0, "Client expired -- %s", client_name);
	    return KRB5KDC_ERR_NAME_EXP;
	}
	
	if (client->pw_end && *client->pw_end < kdc_time
	    && !server->flags.change_pw) {
	    kdc_log(0, "Client's key has expired -- %s", client_name);
	    return KRB5KDC_ERR_KEY_EXPIRED;
	}
    }

    /* check server */
    
    if (server != NULL) {
	if (server->flags.invalid) {
	    kdc_log(0, "Server has invalid flag set -- %s", server_name);
	    return KRB5KDC_ERR_POLICY;
	}

	if(!server->flags.server){
	    kdc_log(0, "Principal may not act as server -- %s", 
		    server_name);
	    return KRB5KDC_ERR_POLICY;
	}

	if(!is_as_req && server->flags.initial) {
	    kdc_log(0, "AS-REQ is required for server -- %s", server_name);
	    return KRB5KDC_ERR_POLICY;
	}

	if (server->valid_start && *server->valid_start > kdc_time) {
	    kdc_log(0, "Server not yet valid -- %s", server_name);
	    return KRB5KDC_ERR_SERVICE_NOTYET;
	}

	if (server->valid_end && *server->valid_end < kdc_time) {
	    kdc_log(0, "Server expired -- %s", server_name);
	    return KRB5KDC_ERR_SERVICE_EXP;
	}

	if (server->pw_end && *server->pw_end < kdc_time) {
	    kdc_log(0, "Server's key has expired -- %s", server_name);
	    return KRB5KDC_ERR_KEY_EXPIRED;
	}
    }
    return 0;
}

/*
 * Return TRUE if `from' is part of `addresses' taking into consideration
 * the configuration variables that tells us how strict we should be about
 * these checks
 */

static krb5_boolean
check_addresses(HostAddresses *addresses, const struct sockaddr *from)
{
    krb5_error_code ret;
    krb5_address addr;
    krb5_boolean result;
    
    if(check_ticket_addresses == 0)
	return TRUE;

    if(addresses == NULL)
	return allow_null_ticket_addresses;
    
    ret = krb5_sockaddr2address (context, from, &addr);
    if(ret)
	return FALSE;

    result = krb5_address_search(context, &addr, addresses);
    krb5_free_address (context, &addr);
    return result;
}

krb5_error_code
as_rep(KDC_REQ *req, 
       krb5_data *reply,
       const char *from,
       struct sockaddr *from_addr)
{
    KDC_REQ_BODY *b = &req->req_body;
    AS_REP rep;
    KDCOptions f = b->kdc_options;
    hdb_entry *client = NULL, *server = NULL;
    krb5_enctype cetype, setype;
    EncTicketPart et;
    EncKDCRepPart ek;
    krb5_principal client_princ = NULL, server_princ = NULL;
    char *client_name = NULL, *server_name = NULL;
    krb5_error_code ret = 0;
    const char *e_text = NULL;
    krb5_crypto crypto;
    Key *ckey, *skey;

    memset(&rep, 0, sizeof(rep));

    if(b->sname == NULL){
	ret = KRB5KRB_ERR_GENERIC;
	e_text = "No server in request";
    } else{
	principalname2krb5_principal (&server_princ, *(b->sname), b->realm);
	ret = krb5_unparse_name(context, server_princ, &server_name);
    }
    if (ret) {
	kdc_log(0, "AS-REQ malformed server name from %s", from);
	goto out;
    }
    
    if(b->cname == NULL){
	ret = KRB5KRB_ERR_GENERIC;
	e_text = "No client in request";
    } else {
	principalname2krb5_principal (&client_princ, *(b->cname), b->realm);
	ret = krb5_unparse_name(context, client_princ, &client_name);
    }
    if (ret) {
	kdc_log(0, "AS-REQ malformed client name from %s", from);
	goto out;
    }

    kdc_log(0, "AS-REQ %s from %s for %s", 
	    client_name, from, server_name);

    ret = db_fetch(client_princ, &client);
    if(ret){
	kdc_log(0, "UNKNOWN -- %s: %s", client_name,
		krb5_get_err_text(context, ret));
	ret = KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN;
	goto out;
    }

    ret = db_fetch(server_princ, &server);
    if(ret){
	kdc_log(0, "UNKNOWN -- %s: %s", server_name,
		krb5_get_err_text(context, ret));
	ret = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
	goto out;
    }

    ret = check_flags(client, client_name, server, server_name, TRUE);
    if(ret)
	goto out;

    memset(&et, 0, sizeof(et));
    memset(&ek, 0, sizeof(ek));

    if(req->padata){
	int i = 0;
	PA_DATA *pa;
	int found_pa = 0;
	kdc_log(5, "Looking for pa-data -- %s", client_name);
	while((pa = find_padata(req, &i, KRB5_PADATA_ENC_TIMESTAMP))){
	    krb5_data ts_data;
	    PA_ENC_TS_ENC p;
	    size_t len;
	    EncryptedData enc_data;
	    Key *pa_key;
	    
	    found_pa = 1;
	    
	    ret = decode_EncryptedData(pa->padata_value.data,
				       pa->padata_value.length,
				       &enc_data,
				       &len);
	    if (ret) {
		ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
		kdc_log(5, "Failed to decode PA-DATA -- %s", 
			client_name);
		goto out;
	    }
	    
	    ret = hdb_enctype2key(context, client, enc_data.etype, &pa_key);
	    if(ret){
		char *estr;
		e_text = "No key matches pa-data";
		ret = KRB5KDC_ERR_PREAUTH_FAILED;
		if(krb5_enctype_to_string(context, enc_data.etype, &estr))
		    estr = NULL;
		if(estr == NULL)
		    kdc_log(5, "No client key matching pa-data (%d) -- %s", 
			    enc_data.etype, client_name);
		else
		    kdc_log(5, "No client key matching pa-data (%s) -- %s", 
			    estr, client_name);
		free(estr);
		    
		free_EncryptedData(&enc_data);
		continue;
	    }

	  try_next_key:
	    ret = krb5_crypto_init(context, &pa_key->key, 0, &crypto);
	    if (ret) {
		kdc_log(0, "krb5_crypto_init failed: %s",
			krb5_get_err_text(context, ret));
		free_EncryptedData(&enc_data);
		continue;
	    }

	    ret = krb5_decrypt_EncryptedData (context,
					      crypto,
					      KRB5_KU_PA_ENC_TIMESTAMP,
					      &enc_data,
					      &ts_data);
	    krb5_crypto_destroy(context, crypto);
	    if(ret){
		if(hdb_next_enctype2key(context, client, 
					enc_data.etype, &pa_key) == 0)
		    goto try_next_key;
		free_EncryptedData(&enc_data);
		e_text = "Failed to decrypt PA-DATA";
		kdc_log (5, "Failed to decrypt PA-DATA -- %s",
			 client_name);
		ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
		continue;
	    }
	    free_EncryptedData(&enc_data);
	    ret = decode_PA_ENC_TS_ENC(ts_data.data,
				       ts_data.length,
				       &p,
				       &len);
	    krb5_data_free(&ts_data);
	    if(ret){
		e_text = "Failed to decode PA-ENC-TS-ENC";
		ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
		kdc_log (5, "Failed to decode PA-ENC-TS_ENC -- %s",
			 client_name);
		continue;
	    }
	    free_PA_ENC_TS_ENC(&p);
	    if (abs(kdc_time - p.patimestamp) > context->max_skew) {
		ret = KRB5KDC_ERR_PREAUTH_FAILED;
		e_text = "Too large time skew";
		kdc_log(0, "Too large time skew -- %s", client_name);
		goto out;
	    }
	    et.flags.pre_authent = 1;
	    kdc_log(2, "Pre-authentication succeded -- %s", client_name);
	    break;
	}
	if(found_pa == 0 && require_preauth)
	    goto use_pa;
	/* We come here if we found a pa-enc-timestamp, but if there
           was some problem with it, other than too large skew */
	if(found_pa && et.flags.pre_authent == 0){
	    kdc_log(0, "%s -- %s", e_text, client_name);
	    e_text = NULL;
	    goto out;
	}
    }else if (require_preauth
	      || client->flags.require_preauth
	      || server->flags.require_preauth) {
	METHOD_DATA method_data;
	PA_DATA *pa;
	unsigned char *buf;
	size_t len;
	krb5_data foo_data;

      use_pa: 
	method_data.len = 0;
	method_data.val = NULL;

	ret = realloc_method_data(&method_data);
	pa = &method_data.val[method_data.len-1];
	pa->padata_type		= KRB5_PADATA_ENC_TIMESTAMP;
	pa->padata_value.length	= 0;
	pa->padata_value.data	= NULL;

	ret = get_pa_etype_info(&method_data, client, 
				b->etype.val, b->etype.len); /* XXX check ret */
	
	ASN1_MALLOC_ENCODE(METHOD_DATA, buf, len, &method_data, &len, ret);
	free_METHOD_DATA(&method_data);
	foo_data.data   = buf;
	foo_data.length = len;
	
	ret = KRB5KDC_ERR_PREAUTH_REQUIRED;
	krb5_mk_error(context,
		      ret,
		      "Need to use PA-ENC-TIMESTAMP",
		      &foo_data,
		      client_princ,
		      server_princ,
		      NULL,
		      NULL,
		      reply);
	free(buf);
	kdc_log(0, "No PA-ENC-TIMESTAMP -- %s", client_name);
	ret = 0;
	goto out2;
    }
    
    ret = find_keys(client, server, &ckey, &cetype, &skey, &setype,
		    b->etype.val, b->etype.len);
    if(ret) {
	kdc_log(0, "Server/client has no support for etypes");
	goto out;
    }
	
    {
	char *cet;
	char *set;

	ret = krb5_enctype_to_string(context, cetype, &cet);
	if(ret == 0) {
	    ret = krb5_enctype_to_string(context, setype, &set);
	    if (ret == 0) {
		kdc_log(5, "Using %s/%s", cet, set);
		free(set);
	    }
	    free(cet);
	}
	if (ret != 0)
	    kdc_log(5, "Using e-types %d/%d", cetype, setype);
    }
    
    {
	char str[128];
	unparse_flags(KDCOptions2int(f), KDCOptions_units, str, sizeof(str));
	if(*str)
	    kdc_log(2, "Requested flags: %s", str);
    }
    

    if(f.renew || f.validate || f.proxy || f.forwarded || f.enc_tkt_in_skey
       || (f.request_anonymous && !allow_anonymous)) {
	ret = KRB5KDC_ERR_BADOPTION;
	kdc_log(0, "Bad KDC options -- %s", client_name);
	goto out;
    }
    
    rep.pvno = 5;
    rep.msg_type = krb_as_rep;
    copy_Realm(&b->realm, &rep.crealm);
    if (f.request_anonymous)
	make_anonymous_principalname (&rep.cname);
    else
	copy_PrincipalName(b->cname, &rep.cname);
    rep.ticket.tkt_vno = 5;
    copy_Realm(&b->realm, &rep.ticket.realm);
    copy_PrincipalName(b->sname, &rep.ticket.sname);

    et.flags.initial = 1;
    if(client->flags.forwardable && server->flags.forwardable)
	et.flags.forwardable = f.forwardable;
    else if (f.forwardable) {
	ret = KRB5KDC_ERR_POLICY;
	kdc_log(0, "Ticket may not be forwardable -- %s", client_name);
	goto out;
    }
    if(client->flags.proxiable && server->flags.proxiable)
	et.flags.proxiable = f.proxiable;
    else if (f.proxiable) {
	ret = KRB5KDC_ERR_POLICY;
	kdc_log(0, "Ticket may not be proxiable -- %s", client_name);
	goto out;
    }
    if(client->flags.postdate && server->flags.postdate)
	et.flags.may_postdate = f.allow_postdate;
    else if (f.allow_postdate){
	ret = KRB5KDC_ERR_POLICY;
	kdc_log(0, "Ticket may not be postdatable -- %s", client_name);
	goto out;
    }

    /* check for valid set of addresses */
    if(!check_addresses(b->addresses, from_addr)) {
	ret = KRB5KRB_AP_ERR_BADADDR;
	kdc_log(0, "Bad address list requested -- %s", client_name);
	goto out;
    }

    krb5_generate_random_keyblock(context, setype, &et.key);
    copy_PrincipalName(&rep.cname, &et.cname);
    copy_Realm(&b->realm, &et.crealm);
    
    {
	time_t start;
	time_t t;
	
	start = et.authtime = kdc_time;
    
	if(f.postdated && req->req_body.from){
	    ALLOC(et.starttime);
	    start = *et.starttime = *req->req_body.from;
	    et.flags.invalid = 1;
	    et.flags.postdated = 1; /* XXX ??? */
	}
	fix_time(&b->till);
	t = *b->till;

	/* be careful not overflowing */

	if(client->max_life)
	    t = start + min(t - start, *client->max_life);
	if(server->max_life)
	    t = start + min(t - start, *server->max_life);
#if 0
	t = min(t, start + realm->max_life);
#endif
	et.endtime = t;
	if(f.renewable_ok && et.endtime < *b->till){
	    f.renewable = 1;
	    if(b->rtime == NULL){
		ALLOC(b->rtime);
		*b->rtime = 0;
	    }
	    if(*b->rtime < *b->till)
		*b->rtime = *b->till;
	}
	if(f.renewable && b->rtime){
	    t = *b->rtime;
	    if(t == 0)
		t = MAX_TIME;
	    if(client->max_renew)
		t = start + min(t - start, *client->max_renew);
	    if(server->max_renew)
		t = start + min(t - start, *server->max_renew);
#if 0
	    t = min(t, start + realm->max_renew);
#endif
	    ALLOC(et.renew_till);
	    *et.renew_till = t;
	    et.flags.renewable = 1;
	}
    }

    if (f.request_anonymous)
	et.flags.anonymous = 1;
    
    if(b->addresses){
	ALLOC(et.caddr);
	copy_HostAddresses(b->addresses, et.caddr);
    }
    
    et.transited.tr_type = DOMAIN_X500_COMPRESS;
    krb5_data_zero(&et.transited.contents); 
     
    copy_EncryptionKey(&et.key, &ek.key);

    /* The MIT ASN.1 library (obviously) doesn't tell lengths encoded
     * as 0 and as 0x80 (meaning indefinite length) apart, and is thus
     * incapable of correctly decoding SEQUENCE OF's of zero length.
     *
     * To fix this, always send at least one no-op last_req
     *
     * If there's a pw_end or valid_end we will use that,
     * otherwise just a dummy lr.
     */
    ek.last_req.val = malloc(2 * sizeof(*ek.last_req.val));
    ek.last_req.len = 0;
    if (client->pw_end
	&& (kdc_warn_pwexpire == 0
	    || kdc_time + kdc_warn_pwexpire <= *client->pw_end)) {
	ek.last_req.val[ek.last_req.len].lr_type  = LR_PW_EXPTIME;
	ek.last_req.val[ek.last_req.len].lr_value = *client->pw_end;
	++ek.last_req.len;
    }
    if (client->valid_end) {
	ek.last_req.val[ek.last_req.len].lr_type  = LR_ACCT_EXPTIME;
	ek.last_req.val[ek.last_req.len].lr_value = *client->valid_end;
	++ek.last_req.len;
    }
    if (ek.last_req.len == 0) {
	ek.last_req.val[ek.last_req.len].lr_type  = LR_NONE;
	ek.last_req.val[ek.last_req.len].lr_value = 0;
	++ek.last_req.len;
    }
    ek.nonce = b->nonce;
    if (client->valid_end || client->pw_end) {
	ALLOC(ek.key_expiration);
	if (client->valid_end) {
	    if (client->pw_end)
		*ek.key_expiration = min(*client->valid_end, *client->pw_end);
	    else
		*ek.key_expiration = *client->valid_end;
	} else
	    *ek.key_expiration = *client->pw_end;
    } else
	ek.key_expiration = NULL;
    ek.flags = et.flags;
    ek.authtime = et.authtime;
    if (et.starttime) {
	ALLOC(ek.starttime);
	*ek.starttime = *et.starttime;
    }
    ek.endtime = et.endtime;
    if (et.renew_till) {
	ALLOC(ek.renew_till);
	*ek.renew_till = *et.renew_till;
    }
    copy_Realm(&rep.ticket.realm, &ek.srealm);
    copy_PrincipalName(&rep.ticket.sname, &ek.sname);
    if(et.caddr){
	ALLOC(ek.caddr);
	copy_HostAddresses(et.caddr, ek.caddr);
    }

    set_salt_padata (&rep.padata, ckey->salt);
    ret = encode_reply(&rep, &et, &ek, setype, server->kvno, &skey->key,
		       client->kvno, &ckey->key, &e_text, reply);
    free_EncTicketPart(&et);
    free_EncKDCRepPart(&ek);
  out:
    free_AS_REP(&rep);
    if(ret){
	krb5_mk_error(context,
		      ret,
		      e_text,
		      NULL,
		      client_princ,
		      server_princ,
		      NULL,
		      NULL,
		      reply);
	ret = 0;
    }
  out2:
    if (client_princ)
	krb5_free_principal(context, client_princ);
    free(client_name);
    if (server_princ)
	krb5_free_principal(context, server_princ);
    free(server_name);
    if(client)
	free_ent(client);
    if(server)
	free_ent(server);
    return ret;
}


static krb5_error_code
check_tgs_flags(KDC_REQ_BODY *b, EncTicketPart *tgt, EncTicketPart *et)
{
    KDCOptions f = b->kdc_options;
	
    if(f.validate){
	if(!tgt->flags.invalid || tgt->starttime == NULL){
	    kdc_log(0, "Bad request to validate ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	if(*tgt->starttime > kdc_time){
	    kdc_log(0, "Early request to validate ticket");
	    return KRB5KRB_AP_ERR_TKT_NYV;
	}
	/* XXX  tkt = tgt */
	et->flags.invalid = 0;
    }else if(tgt->flags.invalid){
	kdc_log(0, "Ticket-granting ticket has INVALID flag set");
	return KRB5KRB_AP_ERR_TKT_INVALID;
    }

    if(f.forwardable){
	if(!tgt->flags.forwardable){
	    kdc_log(0, "Bad request for forwardable ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	et->flags.forwardable = 1;
    }
    if(f.forwarded){
	if(!tgt->flags.forwardable){
	    kdc_log(0, "Request to forward non-forwardable ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	et->flags.forwarded = 1;
	et->caddr = b->addresses;
    }
    if(tgt->flags.forwarded)
	et->flags.forwarded = 1;
	
    if(f.proxiable){
	if(!tgt->flags.proxiable){
	    kdc_log(0, "Bad request for proxiable ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	et->flags.proxiable = 1;
    }
    if(f.proxy){
	if(!tgt->flags.proxiable){
	    kdc_log(0, "Request to proxy non-proxiable ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	et->flags.proxy = 1;
	et->caddr = b->addresses;
    }
    if(tgt->flags.proxy)
	et->flags.proxy = 1;

    if(f.allow_postdate){
	if(!tgt->flags.may_postdate){
	    kdc_log(0, "Bad request for post-datable ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	et->flags.may_postdate = 1;
    }
    if(f.postdated){
	if(!tgt->flags.may_postdate){
	    kdc_log(0, "Bad request for postdated ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	if(b->from)
	    *et->starttime = *b->from;
	et->flags.postdated = 1;
	et->flags.invalid = 1;
    }else if(b->from && *b->from > kdc_time + context->max_skew){
	kdc_log(0, "Ticket cannot be postdated");
	return KRB5KDC_ERR_CANNOT_POSTDATE;
    }

    if(f.renewable){
	if(!tgt->flags.renewable){
	    kdc_log(0, "Bad request for renewable ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	et->flags.renewable = 1;
	ALLOC(et->renew_till);
	fix_time(&b->rtime);
	*et->renew_till = *b->rtime;
    }
    if(f.renew){
	time_t old_life;
	if(!tgt->flags.renewable || tgt->renew_till == NULL){
	    kdc_log(0, "Request to renew non-renewable ticket");
	    return KRB5KDC_ERR_BADOPTION;
	}
	old_life = tgt->endtime;
	if(tgt->starttime)
	    old_life -= *tgt->starttime;
	else
	    old_life -= tgt->authtime;
	et->endtime = *et->starttime + old_life;
	if (et->renew_till != NULL)
	    et->endtime = min(*et->renew_till, et->endtime);
    }	    
    
    /* checks for excess flags */
    if(f.request_anonymous && !allow_anonymous){
	kdc_log(0, "Request for anonymous ticket");
	return KRB5KDC_ERR_BADOPTION;
    }
    return 0;
}

static krb5_error_code
fix_transited_encoding(krb5_boolean check_policy,
		       TransitedEncoding *tr, 
		       EncTicketPart *et, 
		       const char *client_realm, 
		       const char *server_realm, 
		       const char *tgt_realm)
{
    krb5_error_code ret = 0;
    char **realms, **tmp;
    int num_realms;
    int i;

    if(tr->tr_type != DOMAIN_X500_COMPRESS) {
	kdc_log(0, "Unknown transited type: %u", tr->tr_type);
	return KRB5KDC_ERR_TRTYPE_NOSUPP;
    }

    ret = krb5_domain_x500_decode(context, 
				  tr->contents,
				  &realms, 
				  &num_realms,
				  client_realm,
				  server_realm);
    if(ret){
	krb5_warn(context, ret, "Decoding transited encoding");
	return ret;
    }
    if(strcmp(client_realm, tgt_realm) && strcmp(server_realm, tgt_realm)) {
	/* not us, so add the previous realm to transited set */
	if (num_realms < 0 || num_realms + 1 > UINT_MAX/sizeof(*realms)) {
	    ret = ERANGE;
	    goto free_realms;
	}
	tmp = realloc(realms, (num_realms + 1) * sizeof(*realms));
	if(tmp == NULL){
	    ret = ENOMEM;
	    goto free_realms;
	}
	realms = tmp;
	realms[num_realms] = strdup(tgt_realm);
	if(realms[num_realms] == NULL){
	    ret = ENOMEM;
	    goto free_realms;
	}
	num_realms++;
    }
    if(num_realms == 0) {
	if(strcmp(client_realm, server_realm)) 
	    kdc_log(0, "cross-realm %s -> %s", client_realm, server_realm);
    } else {
	size_t l = 0;
	char *rs;
	for(i = 0; i < num_realms; i++)
	    l += strlen(realms[i]) + 2;
	rs = malloc(l);
	if(rs != NULL) {
	    *rs = '\0';
	    for(i = 0; i < num_realms; i++) {
		if(i > 0)
		    strlcat(rs, ", ", l);
		strlcat(rs, realms[i], l);
	    }
	    kdc_log(0, "cross-realm %s -> %s via [%s]", client_realm, server_realm, rs);
	    free(rs);
	}
    }
    if(check_policy) {
	ret = krb5_check_transited(context, client_realm, 
				   server_realm, 
				   realms, num_realms, NULL);
	if(ret) {
	    krb5_warn(context, ret, "cross-realm %s -> %s", 
		      client_realm, server_realm);
	    goto free_realms;
	}
	et->flags.transited_policy_checked = 1;
    }
    et->transited.tr_type = DOMAIN_X500_COMPRESS;
    ret = krb5_domain_x500_encode(realms, num_realms, &et->transited.contents);
    if(ret)
	krb5_warn(context, ret, "Encoding transited encoding");
  free_realms:
    for(i = 0; i < num_realms; i++)
	free(realms[i]);
    free(realms);
    return ret;
}


static krb5_error_code
tgs_make_reply(KDC_REQ_BODY *b, 
	       EncTicketPart *tgt, 
	       EncTicketPart *adtkt, 
	       AuthorizationData *auth_data,
	       hdb_entry *server, 
	       hdb_entry *client, 
	       krb5_principal client_principal, 
	       hdb_entry *krbtgt,
	       krb5_enctype cetype,
	       const char **e_text,
	       krb5_data *reply)
{
    KDC_REP rep;
    EncKDCRepPart ek;
    EncTicketPart et;
    KDCOptions f = b->kdc_options;
    krb5_error_code ret;
    krb5_enctype etype;
    Key *skey;
    EncryptionKey *ekey;
    
    if(adtkt) {
	int i;
	krb5_keytype kt;
	ekey = &adtkt->key;
	for(i = 0; i < b->etype.len; i++){
	    ret = krb5_enctype_to_keytype(context, b->etype.val[i], &kt);
	    if(ret)
		continue;
	    if(adtkt->key.keytype == kt)
		break;
	}
	if(i == b->etype.len)
	    return KRB5KDC_ERR_ETYPE_NOSUPP;
	etype = b->etype.val[i];
    }else{
	ret = find_keys(NULL, server, NULL, NULL, &skey, &etype, 
			b->etype.val, b->etype.len);
	if(ret) {
	    kdc_log(0, "Server has no support for etypes");
	    return ret;
	}
	ekey = &skey->key;
    }
    
    memset(&rep, 0, sizeof(rep));
    memset(&et, 0, sizeof(et));
    memset(&ek, 0, sizeof(ek));
    
    rep.pvno = 5;
    rep.msg_type = krb_tgs_rep;

    et.authtime = tgt->authtime;
    fix_time(&b->till);
    et.endtime = min(tgt->endtime, *b->till);
    ALLOC(et.starttime);
    *et.starttime = kdc_time;
    
    ret = check_tgs_flags(b, tgt, &et);
    if(ret)
	goto out;

    /* We should check the transited encoding if:
       1) the request doesn't ask not to be checked
       2) globally enforcing a check
       3) principal requires checking
       4) we allow non-check per-principal, but principal isn't marked as allowing this
       5) we don't globally allow this
    */

#define GLOBAL_FORCE_TRANSITED_CHECK		(trpolicy == TRPOLICY_ALWAYS_CHECK)
#define GLOBAL_ALLOW_PER_PRINCIPAL		(trpolicy == TRPOLICY_ALLOW_PER_PRINCIPAL)
#define GLOBAL_ALLOW_DISABLE_TRANSITED_CHECK	(trpolicy == TRPOLICY_ALWAYS_HONOUR_REQUEST)
/* these will consult the database in future release */
#define PRINCIPAL_FORCE_TRANSITED_CHECK(P)		0
#define PRINCIPAL_ALLOW_DISABLE_TRANSITED_CHECK(P)	0

    ret = fix_transited_encoding(!f.disable_transited_check ||
				 GLOBAL_FORCE_TRANSITED_CHECK ||
				 PRINCIPAL_FORCE_TRANSITED_CHECK(server) ||
				 !((GLOBAL_ALLOW_PER_PRINCIPAL && 
				    PRINCIPAL_ALLOW_DISABLE_TRANSITED_CHECK(server)) ||
				   GLOBAL_ALLOW_DISABLE_TRANSITED_CHECK),
				 &tgt->transited, &et,
				 *krb5_princ_realm(context, client_principal),
				 *krb5_princ_realm(context, server->principal),
				 *krb5_princ_realm(context, krbtgt->principal));
    if(ret)
	goto out;

    copy_Realm(krb5_princ_realm(context, server->principal), 
	       &rep.ticket.realm);
    krb5_principal2principalname(&rep.ticket.sname, server->principal);
    copy_Realm(&tgt->crealm, &rep.crealm);
    if (f.request_anonymous)
	make_anonymous_principalname (&tgt->cname);
    else
	copy_PrincipalName(&tgt->cname, &rep.cname);
    rep.ticket.tkt_vno = 5;

    ek.caddr = et.caddr;
    if(et.caddr == NULL)
	et.caddr = tgt->caddr;

    {
	time_t life;
	life = et.endtime - *et.starttime;
	if(client && client->max_life)
	    life = min(life, *client->max_life);
	if(server->max_life)
	    life = min(life, *server->max_life);
	et.endtime = *et.starttime + life;
    }
    if(f.renewable_ok && tgt->flags.renewable && 
       et.renew_till == NULL && et.endtime < *b->till){
	et.flags.renewable = 1;
	ALLOC(et.renew_till);
	*et.renew_till = *b->till;
    }
    if(et.renew_till){
	time_t renew;
	renew = *et.renew_till - et.authtime;
	if(client && client->max_renew)
	    renew = min(renew, *client->max_renew);
	if(server->max_renew)
	    renew = min(renew, *server->max_renew);
	*et.renew_till = et.authtime + renew;
    }
	    
    if(et.renew_till){
	*et.renew_till = min(*et.renew_till, *tgt->renew_till);
	*et.starttime = min(*et.starttime, *et.renew_till);
	et.endtime = min(et.endtime, *et.renew_till);
    }
    
    *et.starttime = min(*et.starttime, et.endtime);

    if(*et.starttime == et.endtime){
	ret = KRB5KDC_ERR_NEVER_VALID;
	goto out;
    }
    if(et.renew_till && et.endtime == *et.renew_till){
	free(et.renew_till);
	et.renew_till = NULL;
	et.flags.renewable = 0;
    }
    
    et.flags.pre_authent = tgt->flags.pre_authent;
    et.flags.hw_authent  = tgt->flags.hw_authent;
    et.flags.anonymous   = tgt->flags.anonymous;
	    
    /* XXX Check enc-authorization-data */
    et.authorization_data = auth_data;

    krb5_generate_random_keyblock(context, etype, &et.key);
    et.crealm = tgt->crealm;
    et.cname = tgt->cname;
	    
    ek.key = et.key;
    /* MIT must have at least one last_req */
    ek.last_req.len = 1;
    ek.last_req.val = calloc(1, sizeof(*ek.last_req.val));
    ek.nonce = b->nonce;
    ek.flags = et.flags;
    ek.authtime = et.authtime;
    ek.starttime = et.starttime;
    ek.endtime = et.endtime;
    ek.renew_till = et.renew_till;
    ek.srealm = rep.ticket.realm;
    ek.sname = rep.ticket.sname;
	    
    /* It is somewhat unclear where the etype in the following
       encryption should come from. What we have is a session
       key in the passed tgt, and a list of preferred etypes
       *for the new ticket*. Should we pick the best possible
       etype, given the keytype in the tgt, or should we look
       at the etype list here as well?  What if the tgt
       session key is DES3 and we want a ticket with a (say)
       CAST session key. Should the DES3 etype be added to the
       etype list, even if we don't want a session key with
       DES3? */
    ret = encode_reply(&rep, &et, &ek, etype, adtkt ? 0 : server->kvno, ekey,
		       0, &tgt->key, e_text, reply);
  out:
    free_TGS_REP(&rep);
    free_TransitedEncoding(&et.transited);
    if(et.starttime)
	free(et.starttime);
    if(et.renew_till)
	free(et.renew_till);
    free_LastReq(&ek.last_req);
    memset(et.key.keyvalue.data, 0, et.key.keyvalue.length);
    free_EncryptionKey(&et.key);
    return ret;
}

static krb5_error_code
tgs_check_authenticator(krb5_auth_context ac,
			KDC_REQ_BODY *b, 
			const char **e_text,
			krb5_keyblock *key)
{
    krb5_authenticator auth;
    size_t len;
    unsigned char *buf;
    size_t buf_size;
    krb5_error_code ret;
    krb5_crypto crypto;
    
    krb5_auth_con_getauthenticator(context, ac, &auth);
    if(auth->cksum == NULL){
	kdc_log(0, "No authenticator in request");
	ret = KRB5KRB_AP_ERR_INAPP_CKSUM;
	goto out;
    }
    /*
     * according to RFC1510 it doesn't need to be keyed,
     * but according to the latest draft it needs to.
     */
    if (
#if 0
!krb5_checksum_is_keyed(context, auth->cksum->cksumtype)
	||
#endif
 !krb5_checksum_is_collision_proof(context, auth->cksum->cksumtype)) {
	kdc_log(0, "Bad checksum type in authenticator: %d", 
		auth->cksum->cksumtype);
	ret =  KRB5KRB_AP_ERR_INAPP_CKSUM;
	goto out;
    }
		
    /* XXX should not re-encode this */
    ASN1_MALLOC_ENCODE(KDC_REQ_BODY, buf, buf_size, b, &len, ret);
    if(ret){
	kdc_log(0, "Failed to encode KDC-REQ-BODY: %s", 
		krb5_get_err_text(context, ret));
	goto out;
    }
    if(buf_size != len) {
	free(buf);
	kdc_log(0, "Internal error in ASN.1 encoder");
	*e_text = "KDC internal error";
	ret = KRB5KRB_ERR_GENERIC;
	goto out;
    }
    ret = krb5_crypto_init(context, key, 0, &crypto);
    if (ret) {
	free(buf);
	kdc_log(0, "krb5_crypto_init failed: %s",
		krb5_get_err_text(context, ret));
	goto out;
    }
    ret = krb5_verify_checksum(context,
			       crypto,
			       KRB5_KU_TGS_REQ_AUTH_CKSUM,
			       buf, 
			       len,
			       auth->cksum);
    free(buf);
    krb5_crypto_destroy(context, crypto);
    if(ret){
	kdc_log(0, "Failed to verify checksum: %s", 
		krb5_get_err_text(context, ret));
    }
out:
    free_Authenticator(auth);
    free(auth);
    return ret;
}

/*
 * return the realm of a krbtgt-ticket or NULL
 */

static Realm 
get_krbtgt_realm(const PrincipalName *p)
{
    if(p->name_string.len == 2
       && strcmp(p->name_string.val[0], KRB5_TGS_NAME) == 0)
	return p->name_string.val[1];
    else
	return NULL;
}

static Realm
find_rpath(Realm crealm, Realm srealm)
{
    const char *new_realm = krb5_config_get_string(context,
						   NULL,
						   "capaths", 
						   crealm,
						   srealm,
						   NULL);
    return (Realm)new_realm;
}
	    

static krb5_boolean
need_referral(krb5_principal server, krb5_realm **realms)
{
    if(server->name.name_type != KRB5_NT_SRV_INST ||
       server->name.name_string.len != 2)
	return FALSE;
 
    return krb5_get_host_realm_int(context, server->name.name_string.val[1],
				   FALSE, realms) == 0;
}

static krb5_error_code
tgs_rep2(KDC_REQ_BODY *b,
	 PA_DATA *tgs_req,
	 krb5_data *reply,
	 const char *from,
	 const struct sockaddr *from_addr,
	 time_t **csec,
	 int **cusec)
{
    krb5_ap_req ap_req;
    krb5_error_code ret;
    krb5_principal princ;
    krb5_auth_context ac = NULL;
    krb5_ticket *ticket = NULL;
    krb5_flags ap_req_options;
    krb5_flags verify_ap_req_flags;
    const char *e_text = NULL;
    krb5_crypto crypto;

    hdb_entry *krbtgt = NULL;
    EncTicketPart *tgt;
    Key *tkey;
    krb5_enctype cetype;
    krb5_principal cp = NULL;
    krb5_principal sp = NULL;
    AuthorizationData *auth_data = NULL;

    *csec  = NULL;
    *cusec = NULL;

    memset(&ap_req, 0, sizeof(ap_req));
    ret = krb5_decode_ap_req(context, &tgs_req->padata_value, &ap_req);
    if(ret){
	kdc_log(0, "Failed to decode AP-REQ: %s", 
		krb5_get_err_text(context, ret));
	goto out2;
    }
    
    if(!get_krbtgt_realm(&ap_req.ticket.sname)){
	/* XXX check for ticket.sname == req.sname */
	kdc_log(0, "PA-DATA is not a ticket-granting ticket");
	ret = KRB5KDC_ERR_POLICY; /* ? */
	goto out2;
    }
    
    principalname2krb5_principal(&princ,
				 ap_req.ticket.sname,
				 ap_req.ticket.realm);
    
    ret = db_fetch(princ, &krbtgt);

    if(ret) {
	char *p;
	ret = krb5_unparse_name(context, princ, &p);
	if (ret != 0)
	    p = "<unparse_name failed>";
	krb5_free_principal(context, princ);
	kdc_log(0, "Ticket-granting ticket not found in database: %s: %s",
		p, krb5_get_err_text(context, ret));
	if (ret == 0)
	    free(p);
	ret = KRB5KRB_AP_ERR_NOT_US;
	goto out2;
    }
    
    if(ap_req.ticket.enc_part.kvno && 
       *ap_req.ticket.enc_part.kvno != krbtgt->kvno){
	char *p;

	ret = krb5_unparse_name (context, princ, &p);
	krb5_free_principal(context, princ);
	if (ret != 0)
	    p = "<unparse_name failed>";
	kdc_log(0, "Ticket kvno = %d, DB kvno = %d (%s)", 
		*ap_req.ticket.enc_part.kvno,
		krbtgt->kvno,
		p);
	if (ret == 0)
	    free (p);
	ret = KRB5KRB_AP_ERR_BADKEYVER;
	goto out2;
    }

    ret = hdb_enctype2key(context, krbtgt, ap_req.ticket.enc_part.etype, &tkey);
    if(ret){
	char *str;
	krb5_enctype_to_string(context, ap_req.ticket.enc_part.etype, &str);
	kdc_log(0, "No server key found for %s", str);
	free(str);
	ret = KRB5KRB_AP_ERR_BADKEYVER;
	goto out2;
    }
    
    if (b->kdc_options.validate)
	verify_ap_req_flags = KRB5_VERIFY_AP_REQ_IGNORE_INVALID;
    else
	verify_ap_req_flags = 0;

    ret = krb5_verify_ap_req2(context,
			      &ac,
			      &ap_req,
			      princ,
			      &tkey->key,
			      verify_ap_req_flags,
			      &ap_req_options,
			      &ticket,
			      KRB5_KU_TGS_REQ_AUTH);
			     
    krb5_free_principal(context, princ);
    if(ret) {
	kdc_log(0, "Failed to verify AP-REQ: %s", 
		krb5_get_err_text(context, ret));
	goto out2;
    }

    {
	krb5_authenticator auth;

	ret = krb5_auth_con_getauthenticator(context, ac, &auth);
	if (ret == 0) {
	    *csec   = malloc(sizeof(**csec));
	    if (*csec == NULL) {
		krb5_free_authenticator(context, &auth);
		kdc_log(0, "malloc failed");
		goto out2;
	    }
	    **csec  = auth->ctime;
	    *cusec  = malloc(sizeof(**cusec));
	    if (*cusec == NULL) {
		krb5_free_authenticator(context, &auth);
		kdc_log(0, "malloc failed");
		goto out2;
	    }
	    **csec  = auth->cusec;
	    krb5_free_authenticator(context, &auth);
	}
    }

    cetype = ap_req.authenticator.etype;

    tgt = &ticket->ticket;

    ret = tgs_check_authenticator(ac, b, &e_text, &tgt->key);

    if (b->enc_authorization_data) {
	krb5_keyblock *subkey;
	krb5_data ad;
	ret = krb5_auth_con_getremotesubkey(context,
					    ac,
					    &subkey);
	if(ret){
	    krb5_auth_con_free(context, ac);
	    kdc_log(0, "Failed to get remote subkey: %s", 
		    krb5_get_err_text(context, ret));
	    goto out2;
	}
	if(subkey == NULL){
	    ret = krb5_auth_con_getkey(context, ac, &subkey);
	    if(ret) {
		krb5_auth_con_free(context, ac);
		kdc_log(0, "Failed to get session key: %s", 
			krb5_get_err_text(context, ret));
		goto out2;
	    }
	}
	if(subkey == NULL){
	    krb5_auth_con_free(context, ac);
	    kdc_log(0, "Failed to get key for enc-authorization-data");
	    ret = KRB5KRB_AP_ERR_BAD_INTEGRITY; /* ? */
	    goto out2;
	}
	ret = krb5_crypto_init(context, subkey, 0, &crypto);
	if (ret) {
	    krb5_auth_con_free(context, ac);
	    kdc_log(0, "krb5_crypto_init failed: %s",
		    krb5_get_err_text(context, ret));
	    goto out2;
	}
	ret = krb5_decrypt_EncryptedData (context,
					  crypto,
					  KRB5_KU_TGS_REQ_AUTH_DAT_SUBKEY,
					  b->enc_authorization_data,
					  &ad);
	krb5_crypto_destroy(context, crypto);
	if(ret){
	    krb5_auth_con_free(context, ac);
	    kdc_log(0, "Failed to decrypt enc-authorization-data");
	    ret = KRB5KRB_AP_ERR_BAD_INTEGRITY; /* ? */
	    goto out2;
	}
	krb5_free_keyblock(context, subkey);
	ALLOC(auth_data);
	ret = decode_AuthorizationData(ad.data, ad.length, auth_data, NULL);
	if(ret){
	    krb5_auth_con_free(context, ac);
	    free(auth_data);
	    auth_data = NULL;
	    kdc_log(0, "Failed to decode authorization data");
	    ret = KRB5KRB_AP_ERR_BAD_INTEGRITY; /* ? */
	    goto out2;
	}
    }

    krb5_auth_con_free(context, ac);

    if(ret){
	kdc_log(0, "Failed to verify authenticator: %s", 
		krb5_get_err_text(context, ret));
	goto out2;
    }
    
    {
	PrincipalName *s;
	Realm r;
	char *spn = NULL, *cpn = NULL;
	hdb_entry *server = NULL, *client = NULL;
	int loop = 0;
	EncTicketPart adtkt;
	char opt_str[128];

	s = b->sname;
	r = b->realm;
	if(b->kdc_options.enc_tkt_in_skey){
	    Ticket *t;
	    hdb_entry *uu;
	    krb5_principal p;
	    Key *tkey;
	    
	    if(b->additional_tickets == NULL || 
	       b->additional_tickets->len == 0){
		ret = KRB5KDC_ERR_BADOPTION; /* ? */
		kdc_log(0, "No second ticket present in request");
		goto out;
	    }
	    t = &b->additional_tickets->val[0];
	    if(!get_krbtgt_realm(&t->sname)){
		kdc_log(0, "Additional ticket is not a ticket-granting ticket");
		ret = KRB5KDC_ERR_POLICY;
		goto out2;
	    }
	    principalname2krb5_principal(&p, t->sname, t->realm);
	    ret = db_fetch(p, &uu);
	    krb5_free_principal(context, p);
	    if(ret){
		if (ret == HDB_ERR_NOENTRY)
		    ret = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
		goto out;
	    }
	    ret = hdb_enctype2key(context, uu, t->enc_part.etype, &tkey);
	    if(ret){
		ret = KRB5KDC_ERR_ETYPE_NOSUPP; /* XXX */
		goto out;
	    }
	    ret = krb5_decrypt_ticket(context, t, &tkey->key, &adtkt, 0);

	    if(ret)
		goto out;
	    s = &adtkt.cname;
	    r = adtkt.crealm;
	}

	principalname2krb5_principal(&sp, *s, r);
	ret = krb5_unparse_name(context, sp, &spn);	
	if (ret)
	    goto out;
	principalname2krb5_principal(&cp, tgt->cname, tgt->crealm);
	ret = krb5_unparse_name(context, cp, &cpn);
	if (ret)
	    goto out;
	unparse_flags (KDCOptions2int(b->kdc_options), KDCOptions_units,
		       opt_str, sizeof(opt_str));
	if(*opt_str)
	    kdc_log(0, "TGS-REQ %s from %s for %s [%s]", 
		    cpn, from, spn, opt_str);
	else
	    kdc_log(0, "TGS-REQ %s from %s for %s", cpn, from, spn);
    server_lookup:
	ret = db_fetch(sp, &server);

	if(ret){
	    Realm req_rlm, new_rlm;
	    krb5_realm *realms;

	    if ((req_rlm = get_krbtgt_realm(&sp->name)) != NULL) {
		if(loop++ < 2) {
		    new_rlm = find_rpath(tgt->crealm, req_rlm);
		    if(new_rlm) {
			kdc_log(5, "krbtgt for realm %s not found, trying %s", 
				req_rlm, new_rlm);
			krb5_free_principal(context, sp);
			free(spn);
			krb5_make_principal(context, &sp, r, 
					    KRB5_TGS_NAME, new_rlm, NULL);
			ret = krb5_unparse_name(context, sp, &spn);	
			if (ret)
			    goto out;
			goto server_lookup;
		    }
		}
	    } else if(need_referral(sp, &realms)) {
		if (strcmp(realms[0], sp->realm) != 0) {
		    kdc_log(5, "returning a referral to realm %s for "
			    "server %s that was not found",
			    realms[0], spn);
		    krb5_free_principal(context, sp);
		    free(spn);
		    krb5_make_principal(context, &sp, r, KRB5_TGS_NAME,
					realms[0], NULL);
		    ret = krb5_unparse_name(context, sp, &spn);
		    if (ret)
			goto out;
		    krb5_free_host_realm(context, realms);
		    goto server_lookup;
		}
		krb5_free_host_realm(context, realms);
	    }
	    kdc_log(0, "Server not found in database: %s: %s", spn,
		    krb5_get_err_text(context, ret));
	    if (ret == HDB_ERR_NOENTRY)
		ret = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
	    goto out;
	}

	ret = db_fetch(cp, &client);
	if(ret)
	    kdc_log(1, "Client not found in database: %s: %s",
		    cpn, krb5_get_err_text(context, ret));
#if 0
	/* XXX check client only if same realm as krbtgt-instance */
	if(ret){
	    kdc_log(0, "Client not found in database: %s: %s",
		    cpn, krb5_get_err_text(context, ret));
	    if (ret == HDB_ERR_NOENTRY)
		ret = KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN;
	    goto out;
	}
#endif

	if(strcmp(krb5_principal_get_realm(context, sp),
		  krb5_principal_get_comp_string(context, krbtgt->principal, 1)) != 0) {
	    char *tpn;
	    ret = krb5_unparse_name(context, krbtgt->principal, &tpn);
	    kdc_log(0, "Request with wrong krbtgt: %s", (ret == 0) ? tpn : "<unknown>");
	    if(ret == 0)
		free(tpn);
	    ret = KRB5KRB_AP_ERR_NOT_US;
	    goto out;
	    
	}

	ret = check_flags(client, cpn, server, spn, FALSE);
	if(ret)
	    goto out;

	if((b->kdc_options.validate || b->kdc_options.renew) && 
	   !krb5_principal_compare(context, 
				   krbtgt->principal,
				   server->principal)){
	    kdc_log(0, "Inconsistent request.");
	    ret = KRB5KDC_ERR_SERVER_NOMATCH;
	    goto out;
	}

	/* check for valid set of addresses */
	if(!check_addresses(tgt->caddr, from_addr)) {
	    ret = KRB5KRB_AP_ERR_BADADDR;
	    kdc_log(0, "Request from wrong address");
	    goto out;
	}
	
	ret = tgs_make_reply(b, 
			     tgt, 
			     b->kdc_options.enc_tkt_in_skey ? &adtkt : NULL, 
			     auth_data,
			     server, 
			     client, 
			     cp, 
			     krbtgt, 
			     cetype, 
			     &e_text,
			     reply);
	
    out:
	free(spn);
	free(cpn);
	    
	if(server)
	    free_ent(server);
	if(client)
	    free_ent(client);
    }
out2:
    if(ret) {
	krb5_mk_error(context,
		      ret,
		      e_text,
		      NULL,
		      cp,
		      sp,
		      NULL,
		      NULL,
		      reply);
	free(*csec);
	free(*cusec);
	*csec  = NULL;
	*cusec = NULL;
    }
    krb5_free_principal(context, cp);
    krb5_free_principal(context, sp);
    if (ticket) {
	krb5_free_ticket(context, ticket);
	free(ticket);
    }
    free_AP_REQ(&ap_req);
    if(auth_data){
	free_AuthorizationData(auth_data);
	free(auth_data);
    }

    if(krbtgt)
	free_ent(krbtgt);

    return ret;
}


krb5_error_code
tgs_rep(KDC_REQ *req, 
	krb5_data *data,
	const char *from,
	struct sockaddr *from_addr)
{
    krb5_error_code ret;
    int i = 0;
    PA_DATA *tgs_req = NULL;
    time_t *csec = NULL;
    int *cusec = NULL;

    if(req->padata == NULL){
	ret = KRB5KDC_ERR_PREAUTH_REQUIRED; /* XXX ??? */
	kdc_log(0, "TGS-REQ from %s without PA-DATA", from);
	goto out;
    }
    
    tgs_req = find_padata(req, &i, KRB5_PADATA_TGS_REQ);

    if(tgs_req == NULL){
	ret = KRB5KDC_ERR_PADATA_TYPE_NOSUPP;
	
	kdc_log(0, "TGS-REQ from %s without PA-TGS-REQ", from);
	goto out;
    }
    ret = tgs_rep2(&req->req_body, tgs_req, data, from, from_addr,
		   &csec, &cusec);
out:
    if(ret && data->data == NULL){
	krb5_mk_error(context,
		      ret,
		      NULL,
		      NULL,
		      NULL,
		      NULL,
		      csec,
		      cusec,
		      data);
    }
    free(csec);
    free(cusec);
    return 0;
}
