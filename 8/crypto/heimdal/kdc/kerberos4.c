/*
 * Copyright (c) 1997 - 2006 Kungliga Tekniska Högskolan
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

#include <krb5-v4compat.h>

RCSID("$Id: kerberos4.c 21577 2007-07-16 08:14:06Z lha $");

#ifndef swap32
static uint32_t
swap32(uint32_t x)
{
    return ((x << 24) & 0xff000000) |
	((x << 8) & 0xff0000) |
	((x >> 8) & 0xff00) |
	((x >> 24) & 0xff);
}
#endif /* swap32 */

int
_kdc_maybe_version4(unsigned char *buf, int len)
{
    return len > 0 && *buf == 4;
}

static void
make_err_reply(krb5_context context, krb5_data *reply,
	       int code, const char *msg)
{
    _krb5_krb_cr_err_reply(context, "", "", "", 
			   kdc_time, code, msg, reply);
}

struct valid_princ_ctx {
    krb5_kdc_configuration *config;
    unsigned flags;
};

static krb5_boolean
valid_princ(krb5_context context,
	    void *funcctx,
	    krb5_principal princ)
{
    struct valid_princ_ctx *ctx = funcctx;
    krb5_error_code ret;
    char *s;
    hdb_entry_ex *ent;

    ret = krb5_unparse_name(context, princ, &s);
    if (ret)
	return FALSE;
    ret = _kdc_db_fetch(context, ctx->config, princ, ctx->flags, NULL, &ent);
    if (ret) {
	kdc_log(context, ctx->config, 7, "Lookup %s failed: %s", s,
		krb5_get_err_text (context, ret));
	free(s);
	return FALSE;
    }
    kdc_log(context, ctx->config, 7, "Lookup %s succeeded", s);
    free(s);
    _kdc_free_ent(context, ent);
    return TRUE;
}

krb5_error_code
_kdc_db_fetch4(krb5_context context,
	       krb5_kdc_configuration *config,
	       const char *name, const char *instance, const char *realm,
	       unsigned flags,
	       hdb_entry_ex **ent)
{
    krb5_principal p;
    krb5_error_code ret;
    struct valid_princ_ctx ctx;

    ctx.config = config;
    ctx.flags = flags;
    
    ret = krb5_425_conv_principal_ext2(context, name, instance, realm, 
				       valid_princ, &ctx, 0, &p);
    if(ret)
	return ret;
    ret = _kdc_db_fetch(context, config, p, flags, NULL, ent);
    krb5_free_principal(context, p);
    return ret;
}

#define RCHECK(X, L) if(X){make_err_reply(context, reply, KFAILURE, "Packet too short"); goto L;}

/*
 * Process the v4 request in `buf, len' (received from `addr'
 * (with string `from').
 * Return an error code and a reply in `reply'.
 */

krb5_error_code
_kdc_do_version4(krb5_context context, 
		 krb5_kdc_configuration *config,
		 unsigned char *buf,
		 size_t len,
		 krb5_data *reply,
		 const char *from,
		 struct sockaddr_in *addr)
{
    krb5_storage *sp;
    krb5_error_code ret;
    hdb_entry_ex *client = NULL, *server = NULL;
    Key *ckey, *skey;
    int8_t pvno;
    int8_t msg_type;
    int lsb;
    char *name = NULL, *inst = NULL, *realm = NULL;
    char *sname = NULL, *sinst = NULL;
    int32_t req_time;
    time_t max_life;
    uint8_t life;
    char client_name[256];
    char server_name[256];

    if(!config->enable_v4) {
	kdc_log(context, config, 0,
		"Rejected version 4 request from %s", from);
	make_err_reply(context, reply, KRB4ET_KDC_GEN_ERR,
		       "Function not enabled");
	return 0;
    }

    sp = krb5_storage_from_mem(buf, len);
    RCHECK(krb5_ret_int8(sp, &pvno), out);
    if(pvno != 4){
	kdc_log(context, config, 0,
		"Protocol version mismatch (krb4) (%d)", pvno);
	make_err_reply(context, reply, KRB4ET_KDC_PKT_VER, "protocol mismatch");
	goto out;
    }
    RCHECK(krb5_ret_int8(sp, &msg_type), out);
    lsb = msg_type & 1;
    msg_type &= ~1;
    switch(msg_type){
    case AUTH_MSG_KDC_REQUEST: {
	krb5_data ticket, cipher;
	krb5_keyblock session;
	
	krb5_data_zero(&ticket);
	krb5_data_zero(&cipher);

	RCHECK(krb5_ret_stringz(sp, &name), out1);
	RCHECK(krb5_ret_stringz(sp, &inst), out1);
	RCHECK(krb5_ret_stringz(sp, &realm), out1);
	RCHECK(krb5_ret_int32(sp, &req_time), out1);
	if(lsb)
	    req_time = swap32(req_time);
	RCHECK(krb5_ret_uint8(sp, &life), out1);
	RCHECK(krb5_ret_stringz(sp, &sname), out1);
	RCHECK(krb5_ret_stringz(sp, &sinst), out1);
	snprintf (client_name, sizeof(client_name),
		  "%s.%s@%s", name, inst, realm);
	snprintf (server_name, sizeof(server_name),
		  "%s.%s@%s", sname, sinst, config->v4_realm);
	
	kdc_log(context, config, 0, "AS-REQ (krb4) %s from %s for %s",
		client_name, from, server_name);

	ret = _kdc_db_fetch4(context, config, name, inst, realm, 
			     HDB_F_GET_CLIENT, &client);
	if(ret) {
	    kdc_log(context, config, 0, "Client not found in database: %s: %s",
		    client_name, krb5_get_err_text(context, ret));
	    make_err_reply(context, reply, KRB4ET_KDC_PR_UNKNOWN,
			   "principal unknown");
	    goto out1;
	}
	ret = _kdc_db_fetch4(context, config, sname, sinst, config->v4_realm,
			     HDB_F_GET_SERVER, &server);
	if(ret){
	    kdc_log(context, config, 0, "Server not found in database: %s: %s",
		    server_name, krb5_get_err_text(context, ret));
	    make_err_reply(context, reply, KRB4ET_KDC_PR_UNKNOWN,
			   "principal unknown");
	    goto out1;
	}

	ret = _kdc_check_flags (context, config, 
				client, client_name,
				server, server_name,
				TRUE);
	if (ret) {
	    /* good error code? */
	    make_err_reply(context, reply, KRB4ET_KDC_NAME_EXP,
			   "operation not allowed");
	    goto out1;
	}

	if (config->enable_v4_per_principal &&
	    client->entry.flags.allow_kerberos4 == 0)
	{
	    kdc_log(context, config, 0,
		    "Per principal Kerberos 4 flag not turned on for %s",
		    client_name);
	    make_err_reply(context, reply, KRB4ET_KDC_NULL_KEY,
			   "allow kerberos4 flag required");
	    goto out1;
	}

	/*
	 * There's no way to do pre-authentication in v4 and thus no
	 * good error code to return if preauthentication is required.
	 */

	if (config->require_preauth
	    || client->entry.flags.require_preauth
	    || server->entry.flags.require_preauth) {
	    kdc_log(context, config, 0,
		    "Pre-authentication required for v4-request: "
		    "%s for %s",
		    client_name, server_name);
	    make_err_reply(context, reply, KRB4ET_KDC_NULL_KEY,
			   "preauth required");
	    goto out1;
	}

	ret = _kdc_get_des_key(context, client, FALSE, FALSE, &ckey);
	if(ret){
	    kdc_log(context, config, 0, "no suitable DES key for client");
	    make_err_reply(context, reply, KRB4ET_KDC_NULL_KEY, 
			   "no suitable DES key for client");
	    goto out1;
	}

#if 0
	/* this is not necessary with the new code in libkrb */
	/* find a properly salted key */
	while(ckey->salt == NULL || ckey->salt->salt.length != 0)
	    ret = hdb_next_keytype2key(context, &client->entry, KEYTYPE_DES, &ckey);
	if(ret){
	    kdc_log(context, config, 0, "No version-4 salted key in database -- %s.%s@%s", 
		    name, inst, realm);
	    make_err_reply(context, reply, KRB4ET_KDC_NULL_KEY, 
			   "No version-4 salted key in database");
	    goto out1;
	}
#endif
	
	ret = _kdc_get_des_key(context, server, TRUE, FALSE, &skey);
	if(ret){
	    kdc_log(context, config, 0, "no suitable DES key for server");
	    make_err_reply(context, reply, KRB4ET_KDC_NULL_KEY, 
			   "no suitable DES key for server");
	    goto out1;
	}

	max_life = _krb5_krb_life_to_time(0, life);
	if(client->entry.max_life)
	    max_life = min(max_life, *client->entry.max_life);
	if(server->entry.max_life)
	    max_life = min(max_life, *server->entry.max_life);

	life = krb_time_to_life(kdc_time, kdc_time + max_life);
    
	ret = krb5_generate_random_keyblock(context,
					    ETYPE_DES_PCBC_NONE,
					    &session);
	if (ret) {
	    make_err_reply(context, reply, KFAILURE,
			   "Not enough random i KDC");
	    goto out1;
	}
	
	ret = _krb5_krb_create_ticket(context,
				      0,
				      name,
				      inst,
				      config->v4_realm,
				      addr->sin_addr.s_addr,
				      &session,
				      life,
				      kdc_time,
				      sname,
				      sinst,
				      &skey->key,
				      &ticket);
	if (ret) {
	    krb5_free_keyblock_contents(context, &session);
	    make_err_reply(context, reply, KFAILURE,
			   "failed to create v4 ticket");
	    goto out1;
	}

	ret = _krb5_krb_create_ciph(context,
				    &session,
				    sname,
				    sinst,
				    config->v4_realm,
				    life,
				    server->entry.kvno % 255,
				    &ticket,
				    kdc_time,
				    &ckey->key,
				    &cipher);
	krb5_free_keyblock_contents(context, &session);
	krb5_data_free(&ticket);
	if (ret) {
	    make_err_reply(context, reply, KFAILURE, 
			   "Failed to create v4 cipher");
	    goto out1;
	}
	
	ret = _krb5_krb_create_auth_reply(context,
					  name,
					  inst,
					  realm,
					  req_time,
					  0,
					  client->entry.pw_end ? *client->entry.pw_end : 0,
					  client->entry.kvno % 256,
					  &cipher,
					  reply);
	krb5_data_free(&cipher);

    out1:
	break;
    }
    case AUTH_MSG_APPL_REQUEST: {
	struct _krb5_krb_auth_data ad;
	int8_t kvno;
	int8_t ticket_len;
	int8_t req_len;
	krb5_data auth;
	int32_t address;
	size_t pos;
	krb5_principal tgt_princ = NULL;
	hdb_entry_ex *tgt = NULL;
	Key *tkey;
	time_t max_end, actual_end, issue_time;
	
	memset(&ad, 0, sizeof(ad));
	krb5_data_zero(&auth);

	RCHECK(krb5_ret_int8(sp, &kvno), out2);
	RCHECK(krb5_ret_stringz(sp, &realm), out2);
	
	ret = krb5_425_conv_principal(context, "krbtgt", realm,
				      config->v4_realm,
				      &tgt_princ);
	if(ret){
	    kdc_log(context, config, 0,
		    "Converting krbtgt principal (krb4): %s", 
		    krb5_get_err_text(context, ret));
	    make_err_reply(context, reply, KFAILURE, 
			   "Failed to convert v4 principal (krbtgt)");
	    goto out2;
	}

	ret = _kdc_db_fetch(context, config, tgt_princ,
			    HDB_F_GET_KRBTGT, NULL, &tgt);
	if(ret){
	    char *s;
	    s = kdc_log_msg(context, config, 0, "Ticket-granting ticket not "
			    "found in database (krb4): krbtgt.%s@%s: %s", 
			    realm, config->v4_realm,
			    krb5_get_err_text(context, ret));
	    make_err_reply(context, reply, KFAILURE, s);
	    free(s);
	    goto out2;
	}
	
	if(tgt->entry.kvno % 256 != kvno){
	    kdc_log(context, config, 0,
		    "tgs-req (krb4) with old kvno %d (current %d) for "
		    "krbtgt.%s@%s", kvno, tgt->entry.kvno % 256, 
		    realm, config->v4_realm);
	    make_err_reply(context, reply, KRB4ET_KDC_AUTH_EXP,
			   "old krbtgt kvno used");
	    goto out2;
	}

	ret = _kdc_get_des_key(context, tgt, TRUE, FALSE, &tkey);
	if(ret){
	    kdc_log(context, config, 0, 
		    "no suitable DES key for krbtgt (krb4)");
	    make_err_reply(context, reply, KRB4ET_KDC_NULL_KEY, 
			   "no suitable DES key for krbtgt");
	    goto out2;
	}

	RCHECK(krb5_ret_int8(sp, &ticket_len), out2);
	RCHECK(krb5_ret_int8(sp, &req_len), out2);
	
	pos = krb5_storage_seek(sp, ticket_len + req_len, SEEK_CUR);
	
	auth.data = buf;
	auth.length = pos;

	if (config->check_ticket_addresses)
	    address = addr->sin_addr.s_addr;
	else
	    address = 0;

	ret = _krb5_krb_rd_req(context, &auth, "krbtgt", realm, 
			       config->v4_realm,
			       address, &tkey->key, &ad);
	if(ret){
	    kdc_log(context, config, 0, "krb_rd_req: %d", ret);
	    make_err_reply(context, reply, ret, "failed to parse request");
	    goto out2;
	}
	
	RCHECK(krb5_ret_int32(sp, &req_time), out2);
	if(lsb)
	    req_time = swap32(req_time);
	RCHECK(krb5_ret_uint8(sp, &life), out2);
	RCHECK(krb5_ret_stringz(sp, &sname), out2);
	RCHECK(krb5_ret_stringz(sp, &sinst), out2);
	snprintf (server_name, sizeof(server_name),
		  "%s.%s@%s",
		  sname, sinst, config->v4_realm);
	snprintf (client_name, sizeof(client_name),
		  "%s.%s@%s",
		  ad.pname, ad.pinst, ad.prealm);

	kdc_log(context, config, 0, "TGS-REQ (krb4) %s from %s for %s",
		client_name, from, server_name);
	
	if(strcmp(ad.prealm, realm)){
	    kdc_log(context, config, 0, 
		    "Can't hop realms (krb4) %s -> %s", realm, ad.prealm);
	    make_err_reply(context, reply, KRB4ET_KDC_PR_UNKNOWN, 
			   "Can't hop realms");
	    goto out2;
	}

	if (!config->enable_v4_cross_realm && strcmp(realm, config->v4_realm) != 0) {
	    kdc_log(context, config, 0, 
		    "krb4 Cross-realm %s -> %s disabled",
		    realm, config->v4_realm);
	    make_err_reply(context, reply, KRB4ET_KDC_PR_UNKNOWN,
			   "Can't hop realms");
	    goto out2;
	}

	if(strcmp(sname, "changepw") == 0){
	    kdc_log(context, config, 0, 
		    "Bad request for changepw ticket (krb4)");
	    make_err_reply(context, reply, KRB4ET_KDC_PR_UNKNOWN, 
			   "Can't authorize password change based on TGT");
	    goto out2;
	}
	
	ret = _kdc_db_fetch4(context, config, ad.pname, ad.pinst, ad.prealm,
			     HDB_F_GET_CLIENT, &client);
	if(ret && ret != HDB_ERR_NOENTRY) {
	    char *s;
	    s = kdc_log_msg(context, config, 0,
			    "Client not found in database: (krb4) %s: %s",
			    client_name, krb5_get_err_text(context, ret));
	    make_err_reply(context, reply, KRB4ET_KDC_PR_UNKNOWN, s);
	    free(s);
	    goto out2;
	}
	if (client == NULL && strcmp(ad.prealm, config->v4_realm) == 0) {
	    char *s;
	    s = kdc_log_msg(context, config, 0,
			    "Local client not found in database: (krb4) "
			    "%s", client_name);
	    make_err_reply(context, reply, KRB4ET_KDC_PR_UNKNOWN, s);
	    free(s);
	    goto out2;
	}

	ret = _kdc_db_fetch4(context, config, sname, sinst, config->v4_realm,
			     HDB_F_GET_SERVER, &server);
	if(ret){
	    char *s;
	    s = kdc_log_msg(context, config, 0,
			    "Server not found in database (krb4): %s: %s",
			    server_name, krb5_get_err_text(context, ret));
	    make_err_reply(context, reply, KRB4ET_KDC_PR_UNKNOWN, s);
	    free(s);
	    goto out2;
	}

	ret = _kdc_check_flags (context, config, 
				client, client_name,
				server, server_name,
				FALSE);
	if (ret) {
	    make_err_reply(context, reply, KRB4ET_KDC_NAME_EXP,
			   "operation not allowed");
	    goto out2;
	}

	ret = _kdc_get_des_key(context, server, TRUE, FALSE, &skey);
	if(ret){
	    kdc_log(context, config, 0, 
		    "no suitable DES key for server (krb4)");
	    make_err_reply(context, reply, KRB4ET_KDC_NULL_KEY, 
			   "no suitable DES key for server");
	    goto out2;
	}

	max_end = _krb5_krb_life_to_time(ad.time_sec, ad.life);
	max_end = min(max_end, _krb5_krb_life_to_time(kdc_time, life));
	if(server->entry.max_life)
	    max_end = min(max_end, kdc_time + *server->entry.max_life);
	if(client && client->entry.max_life)
	    max_end = min(max_end, kdc_time + *client->entry.max_life);
	life = min(life, krb_time_to_life(kdc_time, max_end));
	
	issue_time = kdc_time;
	actual_end = _krb5_krb_life_to_time(issue_time, life);
	while (actual_end > max_end && life > 1) {
	    /* move them into the next earlier lifetime bracket */
	    life--;
	    actual_end = _krb5_krb_life_to_time(issue_time, life);
	}
	if (actual_end > max_end) {
	    /* if life <= 1 and it's still too long, backdate the ticket */
	    issue_time -= actual_end - max_end;
	}

	{
	    krb5_data ticket, cipher;
	    krb5_keyblock session;

	    krb5_data_zero(&ticket);
	    krb5_data_zero(&cipher);

	    ret = krb5_generate_random_keyblock(context,
						ETYPE_DES_PCBC_NONE,
						&session);
	    if (ret) {
		make_err_reply(context, reply, KFAILURE,
			       "Not enough random i KDC");
		goto out2;
	    }
	
	    ret = _krb5_krb_create_ticket(context,
					  0,
					  ad.pname,
					  ad.pinst,
					  ad.prealm,
					  addr->sin_addr.s_addr,
					  &session,
					  life,
					  issue_time,
					  sname,
					  sinst,
					  &skey->key,
					  &ticket);
	    if (ret) {
		krb5_free_keyblock_contents(context, &session);
		make_err_reply(context, reply, KFAILURE,
			       "failed to create v4 ticket");
		goto out2;
	    }

	    ret = _krb5_krb_create_ciph(context,
					&session,
					sname,
					sinst,
					config->v4_realm,
					life,
					server->entry.kvno % 255,
					&ticket,
					issue_time,
					&ad.session,
					&cipher);
	    krb5_free_keyblock_contents(context, &session);
	    if (ret) {
		make_err_reply(context, reply, KFAILURE,
			       "failed to create v4 cipher");
		goto out2;
	    }
	    
	    ret = _krb5_krb_create_auth_reply(context,
					      ad.pname,
					      ad.pinst,
					      ad.prealm,
					      req_time,
					      0,
					      0,
					      0,
					      &cipher,
					      reply);
	    krb5_data_free(&cipher);
	}
    out2:
	_krb5_krb_free_auth_data(context, &ad);
	if(tgt_princ)
	    krb5_free_principal(context, tgt_princ);
	if(tgt)
	    _kdc_free_ent(context, tgt);
	break;
    }
    case AUTH_MSG_ERR_REPLY:
	break;
    default:
	kdc_log(context, config, 0, "Unknown message type (krb4): %d from %s", 
		msg_type, from);
	
	make_err_reply(context, reply, KFAILURE, "Unknown message type");
    }
 out:
    if(name)
	free(name);
    if(inst)
	free(inst);
    if(realm)
	free(realm);
    if(sname)
	free(sname);
    if(sinst)
	free(sinst);
    if(client)
	_kdc_free_ent(context, client);
    if(server)
	_kdc_free_ent(context, server);
    krb5_storage_free(sp);
    return 0;
}

krb5_error_code
_kdc_encode_v4_ticket(krb5_context context, 
		      krb5_kdc_configuration *config,
		      void *buf, size_t len, const EncTicketPart *et,
		      const PrincipalName *service, size_t *size)
{
    krb5_storage *sp;
    krb5_error_code ret;
    char name[40], inst[40], realm[40];
    char sname[40], sinst[40];

    {
	krb5_principal princ;
	_krb5_principalname2krb5_principal(context,
					   &princ,
					   *service,
					   et->crealm);
	ret = krb5_524_conv_principal(context, 
				      princ,
				      sname,
				      sinst,
				      realm);
	krb5_free_principal(context, princ);
	if(ret)
	    return ret;

	_krb5_principalname2krb5_principal(context,
					   &princ,
					   et->cname,
					   et->crealm);
				     
	ret = krb5_524_conv_principal(context, 
				      princ,
				      name,
				      inst,
				      realm);
	krb5_free_principal(context, princ);
    }
    if(ret)
	return ret;

    sp = krb5_storage_emem();
    
    krb5_store_int8(sp, 0); /* flags */
    krb5_store_stringz(sp, name);
    krb5_store_stringz(sp, inst);
    krb5_store_stringz(sp, realm);
    {
	unsigned char tmp[4] = { 0, 0, 0, 0 };
	int i;
	if(et->caddr){
	    for(i = 0; i < et->caddr->len; i++)
		if(et->caddr->val[i].addr_type == AF_INET &&
		   et->caddr->val[i].address.length == 4){
		    memcpy(tmp, et->caddr->val[i].address.data, 4);
		    break;
		}
	}
	krb5_storage_write(sp, tmp, sizeof(tmp));
    }

    if((et->key.keytype != ETYPE_DES_CBC_MD5 &&
	et->key.keytype != ETYPE_DES_CBC_MD4 &&
	et->key.keytype != ETYPE_DES_CBC_CRC) || 
       et->key.keyvalue.length != 8)
	return -1;
    krb5_storage_write(sp, et->key.keyvalue.data, 8);
    
    {
	time_t start = et->starttime ? *et->starttime : et->authtime;
	krb5_store_int8(sp, krb_time_to_life(start, et->endtime));
	krb5_store_int32(sp, start);
    }

    krb5_store_stringz(sp, sname);
    krb5_store_stringz(sp, sinst);
    
    {
	krb5_data data;
	krb5_storage_to_data(sp, &data);
	krb5_storage_free(sp);
	*size = (data.length + 7) & ~7; /* pad to 8 bytes */
	if(*size > len)
	    return -1;
	memset((unsigned char*)buf - *size + 1, 0, *size);
	memcpy((unsigned char*)buf - *size + 1, data.data, data.length);
	krb5_data_free(&data);
    }
    return 0;
}

krb5_error_code
_kdc_get_des_key(krb5_context context, 
		 hdb_entry_ex *principal, krb5_boolean is_server, 
		 krb5_boolean prefer_afs_key, Key **ret_key)
{
    Key *v5_key = NULL, *v4_key = NULL, *afs_key = NULL, *server_key = NULL;
    int i;
    krb5_enctype etypes[] = { ETYPE_DES_CBC_MD5, 
			      ETYPE_DES_CBC_MD4, 
			      ETYPE_DES_CBC_CRC };

    for(i = 0;
	i < sizeof(etypes)/sizeof(etypes[0])
	    && (v5_key == NULL || v4_key == NULL || 
		afs_key == NULL || server_key == NULL);
	++i) {
	Key *key = NULL;
	while(hdb_next_enctype2key(context, &principal->entry, etypes[i], &key) == 0) {
	    if(key->salt == NULL) {
		if(v5_key == NULL)
		    v5_key = key;
	    } else if(key->salt->type == hdb_pw_salt && 
		      key->salt->salt.length == 0) {
		if(v4_key == NULL)
		    v4_key = key;
	    } else if(key->salt->type == hdb_afs3_salt) {
		if(afs_key == NULL)
		    afs_key = key;
	    } else if(server_key == NULL)
		server_key = key;
	}
    }

    if(prefer_afs_key) {
	if(afs_key)
	    *ret_key = afs_key;
	else if(v4_key)
	    *ret_key = v4_key;
	else if(v5_key)
	    *ret_key = v5_key;
	else if(is_server && server_key)
	    *ret_key = server_key;
	else
	    return KRB4ET_KDC_NULL_KEY;
    } else {
	if(v4_key)
	    *ret_key = v4_key;
	else if(afs_key)
	    *ret_key = afs_key;
	else  if(v5_key)
	    *ret_key = v5_key;
	else if(is_server && server_key)
	    *ret_key = server_key;
	else
	    return KRB4ET_KDC_NULL_KEY;
    }

    if((*ret_key)->key.keyvalue.length == 0)
	return KRB4ET_KDC_NULL_KEY;
    return 0;
}

