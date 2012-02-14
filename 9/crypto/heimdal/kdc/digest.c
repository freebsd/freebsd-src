/*
 * Copyright (c) 2006 - 2007 Kungliga Tekniska Högskolan
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
#include <hex.h>

RCSID("$Id: digest.c 22374 2007-12-28 18:36:52Z lha $");

#define MS_CHAP_V2	0x20
#define CHAP_MD5	0x10
#define DIGEST_MD5	0x08
#define NTLM_V2		0x04
#define NTLM_V1_SESSION	0x02
#define NTLM_V1		0x01

const struct units _kdc_digestunits[] = {
	{"ms-chap-v2",		1U << 5},
	{"chap-md5",		1U << 4},
	{"digest-md5",		1U << 3},
	{"ntlm-v2",		1U << 2},
	{"ntlm-v1-session",	1U << 1},
	{"ntlm-v1",		1U << 0},
	{NULL,	0}
};


static krb5_error_code
get_digest_key(krb5_context context,
	       krb5_kdc_configuration *config,
	       hdb_entry_ex *server,
	       krb5_crypto *crypto)
{
    krb5_error_code ret;
    krb5_enctype enctype;
    Key *key;
    
    ret = _kdc_get_preferred_key(context,
				 config,
				 server,
				 "digest-service",
				 &enctype,
				 &key);
    if (ret)
	return ret;
    return krb5_crypto_init(context, &key->key, 0, crypto);
}

/*
 *
 */

static char *
get_ntlm_targetname(krb5_context context,
		    hdb_entry_ex *client)
{
    char *targetname, *p;

    targetname = strdup(krb5_principal_get_realm(context,
						 client->entry.principal));
    if (targetname == NULL)
	return NULL;

    p = strchr(targetname, '.');
    if (p)
	*p = '\0';

    strupr(targetname);
    return targetname;
}

static krb5_error_code
fill_targetinfo(krb5_context context,
		char *targetname,
		hdb_entry_ex *client,
		krb5_data *data)
{
    struct ntlm_targetinfo ti;
    krb5_error_code ret;
    struct ntlm_buf d;
    krb5_principal p;
    const char *str;

    memset(&ti, 0, sizeof(ti));

    ti.domainname = targetname;
    p = client->entry.principal;
    str = krb5_principal_get_comp_string(context, p, 0);
    if (str != NULL && 
	(strcmp("host", str) == 0 || 
	 strcmp("ftp", str) == 0 ||
	 strcmp("imap", str) == 0 ||
	 strcmp("pop", str) == 0 ||
	 strcmp("smtp", str)))
    {
	str = krb5_principal_get_comp_string(context, p, 1);
	ti.dnsservername = rk_UNCONST(str);
    }
    
    ret = heim_ntlm_encode_targetinfo(&ti, 1, &d);
    if (ret)
	return ret;

    data->data = d.data;
    data->length = d.length;

    return 0;
}


static const unsigned char ms_chap_v2_magic1[39] = {
    0x4D, 0x61, 0x67, 0x69, 0x63, 0x20, 0x73, 0x65, 0x72, 0x76,
    0x65, 0x72, 0x20, 0x74, 0x6F, 0x20, 0x63, 0x6C, 0x69, 0x65,
    0x6E, 0x74, 0x20, 0x73, 0x69, 0x67, 0x6E, 0x69, 0x6E, 0x67,
    0x20, 0x63, 0x6F, 0x6E, 0x73, 0x74, 0x61, 0x6E, 0x74
};
static const unsigned char ms_chap_v2_magic2[41] = {
    0x50, 0x61, 0x64, 0x20, 0x74, 0x6F, 0x20, 0x6D, 0x61, 0x6B,
    0x65, 0x20, 0x69, 0x74, 0x20, 0x64, 0x6F, 0x20, 0x6D, 0x6F,
    0x72, 0x65, 0x20, 0x74, 0x68, 0x61, 0x6E, 0x20, 0x6F, 0x6E,
    0x65, 0x20, 0x69, 0x74, 0x65, 0x72, 0x61, 0x74, 0x69, 0x6F,
    0x6E
};
static const unsigned char ms_rfc3079_magic1[27] = {
    0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74,
    0x68, 0x65, 0x20, 0x4d, 0x50, 0x50, 0x45, 0x20, 0x4d,
    0x61, 0x73, 0x74, 0x65, 0x72, 0x20, 0x4b, 0x65, 0x79
};

/*
 *
 */

static krb5_error_code
get_password_entry(krb5_context context,
		   krb5_kdc_configuration *config,
		   const char *username,
		   char **password)
{
    krb5_principal clientprincipal;
    krb5_error_code ret;
    hdb_entry_ex *user;
    HDB *db;

    /* get username */
    ret = krb5_parse_name(context, username, &clientprincipal);
    if (ret)
	return ret;

    ret = _kdc_db_fetch(context, config, clientprincipal,
			HDB_F_GET_CLIENT, &db, &user);
    krb5_free_principal(context, clientprincipal);
    if (ret)
	return ret;

    ret = hdb_entry_get_password(context, db, &user->entry, password);
    if (ret || password == NULL) {
	if (ret == 0) {
	    ret = EINVAL;
	    krb5_set_error_string(context, "password missing");
	}
	memset(user, 0, sizeof(*user));
    }
    _kdc_free_ent (context, user);
    return ret;
}

/*
 *
 */

krb5_error_code
_kdc_do_digest(krb5_context context, 
	       krb5_kdc_configuration *config,
	       const DigestREQ *req, krb5_data *reply,
	       const char *from, struct sockaddr *addr)
{
    krb5_error_code ret = 0;
    krb5_ticket *ticket = NULL;
    krb5_auth_context ac = NULL;
    krb5_keytab id = NULL;
    krb5_crypto crypto = NULL;
    DigestReqInner ireq;
    DigestRepInner r;
    DigestREP rep;
    krb5_flags ap_req_options;
    krb5_data buf;
    size_t size;
    krb5_storage *sp = NULL;
    Checksum res;
    hdb_entry_ex *server = NULL, *user = NULL;
    hdb_entry_ex *client = NULL;
    char *client_name = NULL, *password = NULL;
    krb5_data serverNonce;

    if(!config->enable_digest) {
	kdc_log(context, config, 0, 
		"Rejected digest request (disabled) from %s", from);
	return KRB5KDC_ERR_POLICY;
    }

    krb5_data_zero(&buf);
    krb5_data_zero(reply);
    krb5_data_zero(&serverNonce);
    memset(&ireq, 0, sizeof(ireq));
    memset(&r, 0, sizeof(r));
    memset(&rep, 0, sizeof(rep));

    kdc_log(context, config, 0, "Digest request from %s", from);

    ret = krb5_kt_resolve(context, "HDB:", &id);
    if (ret) {
	kdc_log(context, config, 0, "Can't open database for digest");
	goto out;
    }

    ret = krb5_rd_req(context, 
		      &ac,
		      &req->apReq,
		      NULL,
		      id,
		      &ap_req_options,
		      &ticket);
    if (ret)
	goto out;

    /* check the server principal in the ticket matches digest/R@R */
    {
	krb5_principal principal = NULL;
	const char *p, *r;

	ret = krb5_ticket_get_server(context, ticket, &principal);
	if (ret)
	    goto out;

	ret = EINVAL;
	krb5_set_error_string(context, "Wrong digest server principal used");
	p = krb5_principal_get_comp_string(context, principal, 0);
	if (p == NULL) {
	    krb5_free_principal(context, principal);
	    goto out;
	}
	if (strcmp(p, KRB5_DIGEST_NAME) != 0) {
	    krb5_free_principal(context, principal);
	    goto out;
	}

	p = krb5_principal_get_comp_string(context, principal, 1);
	if (p == NULL) {
	    krb5_free_principal(context, principal);
	    goto out;
	}
	r = krb5_principal_get_realm(context, principal);
	if (r == NULL) {
	    krb5_free_principal(context, principal);
	    goto out;
	}
	if (strcmp(p, r) != 0) {
	    krb5_free_principal(context, principal);
	    goto out;
	}
	krb5_clear_error_string(context);

	ret = _kdc_db_fetch(context, config, principal,
			    HDB_F_GET_SERVER, NULL, &server);
	if (ret)
	    goto out;

	krb5_free_principal(context, principal);
    }

    /* check the client is allowed to do digest auth */
    {
	krb5_principal principal = NULL;

	ret = krb5_ticket_get_client(context, ticket, &principal);
	if (ret)
	    goto out;

	ret = krb5_unparse_name(context, principal, &client_name);
	if (ret) {
	    krb5_free_principal(context, principal);
	    goto out;
	}

	ret = _kdc_db_fetch(context, config, principal,
			    HDB_F_GET_CLIENT, NULL, &client);
	krb5_free_principal(context, principal);
	if (ret)
	    goto out;

	if (client->entry.flags.allow_digest == 0) {
	    kdc_log(context, config, 0, 
		    "Client %s tried to use digest "
		    "but is not allowed to", 
		    client_name);
	    krb5_set_error_string(context, 
				  "Client is not permitted to use digest");
	    ret = KRB5KDC_ERR_POLICY;
	    goto out;
	}
    }

    /* unpack request */
    {
	krb5_keyblock *key;

	ret = krb5_auth_con_getremotesubkey(context, ac, &key);
	if (ret)
	    goto out;
	if (key == NULL) {
	    krb5_set_error_string(context, "digest: remote subkey not found");
	    ret = EINVAL;
	    goto out;
	}

	ret = krb5_crypto_init(context, key, 0, &crypto);
	krb5_free_keyblock (context, key);
	if (ret)
	    goto out;
    }

    ret = krb5_decrypt_EncryptedData(context, crypto, KRB5_KU_DIGEST_ENCRYPT,
				     &req->innerReq, &buf);
    krb5_crypto_destroy(context, crypto);
    crypto = NULL;
    if (ret)
	goto out;
	   
    ret = decode_DigestReqInner(buf.data, buf.length, &ireq, NULL);
    krb5_data_free(&buf);
    if (ret) {
	krb5_set_error_string(context, "Failed to decode digest inner request");
	goto out;
    }

    kdc_log(context, config, 0, "Valid digest request from %s (%s)", 
	    client_name, from);

    /*
     * Process the inner request
     */

    switch (ireq.element) {
    case choice_DigestReqInner_init: {
	unsigned char server_nonce[16], identifier;

	RAND_pseudo_bytes(&identifier, sizeof(identifier));
	RAND_pseudo_bytes(server_nonce, sizeof(server_nonce));

	server_nonce[0] = kdc_time & 0xff;
	server_nonce[1] = (kdc_time >> 8) & 0xff;
	server_nonce[2] = (kdc_time >> 16) & 0xff;
	server_nonce[3] = (kdc_time >> 24) & 0xff;

	r.element = choice_DigestRepInner_initReply;

	hex_encode(server_nonce, sizeof(server_nonce), &r.u.initReply.nonce);
	if (r.u.initReply.nonce == NULL) {
	    krb5_set_error_string(context, "Failed to decode server nonce");
	    ret = ENOMEM;
	    goto out;
	}

	sp = krb5_storage_emem();
	if (sp == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_string(context, "out of memory");
	    goto out;
	}
	ret = krb5_store_stringz(sp, ireq.u.init.type);
	if (ret) {
	    krb5_clear_error_string(context);
	    goto out;
	}

	if (ireq.u.init.channel) {
	    char *s;

	    asprintf(&s, "%s-%s:%s", r.u.initReply.nonce,
		     ireq.u.init.channel->cb_type,
		     ireq.u.init.channel->cb_binding);
	    if (s == NULL) {
		krb5_set_error_string(context, "Failed to allocate "
				      "channel binding");
		ret = ENOMEM;
		goto out;
	    }
	    free(r.u.initReply.nonce);
	    r.u.initReply.nonce = s;
	}
	
	ret = krb5_store_stringz(sp, r.u.initReply.nonce);
	if (ret) {
	    krb5_clear_error_string(context);
	    goto out;
	}

	if (strcasecmp(ireq.u.init.type, "CHAP") == 0) {
	    r.u.initReply.identifier = 
		malloc(sizeof(*r.u.initReply.identifier));
	    if (r.u.initReply.identifier == NULL) {
		krb5_set_error_string(context, "out of memory");
		ret = ENOMEM;
		goto out;
	    }

	    asprintf(r.u.initReply.identifier, "%02X", identifier & 0xff);
	    if (*r.u.initReply.identifier == NULL) {
		krb5_set_error_string(context, "out of memory");
		ret = ENOMEM;
		goto out;
	    }

	} else
	    r.u.initReply.identifier = NULL;

	if (ireq.u.init.hostname) {
	    ret = krb5_store_stringz(sp, *ireq.u.init.hostname);
	    if (ret) {
		krb5_clear_error_string(context);
		goto out;
	    }
	}

	ret = krb5_storage_to_data(sp, &buf);
	if (ret) {
	    krb5_clear_error_string(context);
	    goto out;
	}

	ret = get_digest_key(context, config, server, &crypto);
	if (ret)
	    goto out;

	ret = krb5_create_checksum(context,
				   crypto,
				   KRB5_KU_DIGEST_OPAQUE,
				   0,
				   buf.data,
				   buf.length,
				   &res);
	krb5_crypto_destroy(context, crypto);
	crypto = NULL;
	krb5_data_free(&buf);
	if (ret)
	    goto out;
	
	ASN1_MALLOC_ENCODE(Checksum, buf.data, buf.length, &res, &size, ret);
	free_Checksum(&res);
	if (ret) {
	    krb5_set_error_string(context, "Failed to encode "
				  "checksum in digest request");
	    goto out;
	}
	if (size != buf.length)
	    krb5_abortx(context, "ASN1 internal error");

	hex_encode(buf.data, buf.length, &r.u.initReply.opaque);
	free(buf.data);
	if (r.u.initReply.opaque == NULL) {
	    krb5_clear_error_string(context);
	    ret = ENOMEM;
	    goto out;
	}

	kdc_log(context, config, 0, "Digest %s init request successful from %s",
		ireq.u.init.type, from);

	break;
    }
    case choice_DigestReqInner_digestRequest: {
	sp = krb5_storage_emem();
	if (sp == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_string(context, "out of memory");
	    goto out;
	}
	ret = krb5_store_stringz(sp, ireq.u.digestRequest.type);
	if (ret) {
	    krb5_clear_error_string(context);
	    goto out;
	}

	krb5_store_stringz(sp, ireq.u.digestRequest.serverNonce);

	if (ireq.u.digestRequest.hostname) {
	    ret = krb5_store_stringz(sp, *ireq.u.digestRequest.hostname);
	    if (ret) {
		krb5_clear_error_string(context);
		goto out;
	    }
	}

	buf.length = strlen(ireq.u.digestRequest.opaque);
	buf.data = malloc(buf.length);
	if (buf.data == NULL) {
	    krb5_set_error_string(context, "out of memory");
	    ret = ENOMEM;
	    goto out;
	}

	ret = hex_decode(ireq.u.digestRequest.opaque, buf.data, buf.length);
	if (ret <= 0) {
	    krb5_set_error_string(context, "Failed to decode opaque");
	    ret = ENOMEM;
	    goto out;
	}
	buf.length = ret;

	ret = decode_Checksum(buf.data, buf.length, &res, NULL);
	free(buf.data);
	if (ret) {
	    krb5_set_error_string(context, "Failed to decode digest Checksum");
	    goto out;
	}
	
	ret = krb5_storage_to_data(sp, &buf);
	if (ret) {
	    krb5_clear_error_string(context);
	    goto out;
	}

	serverNonce.length = strlen(ireq.u.digestRequest.serverNonce);
	serverNonce.data = malloc(serverNonce.length);
	if (serverNonce.data == NULL) {
	    krb5_set_error_string(context, "out of memory");
	    ret = ENOMEM;
	    goto out;
	}
	    
	/*
	 * CHAP does the checksum of the raw nonce, but do it for all
	 * types, since we need to check the timestamp.
	 */
	{
	    ssize_t ssize;
	    
	    ssize = hex_decode(ireq.u.digestRequest.serverNonce, 
			       serverNonce.data, serverNonce.length);
	    if (ssize <= 0) {
		krb5_set_error_string(context, "Failed to decode serverNonce");
		ret = ENOMEM;
		goto out;
	    }
	    serverNonce.length = ssize;
	}

	ret = get_digest_key(context, config, server, &crypto);
	if (ret)
	    goto out;

	ret = krb5_verify_checksum(context, crypto, 
				   KRB5_KU_DIGEST_OPAQUE,
				   buf.data, buf.length, &res);
	krb5_crypto_destroy(context, crypto);
	crypto = NULL;
	if (ret)
	    goto out;

	/* verify time */
	{
	    unsigned char *p = serverNonce.data;
	    uint32_t t;
	    
	    if (serverNonce.length < 4) {
		krb5_set_error_string(context, "server nonce too short");
		ret = EINVAL;
		goto out;
	    }
	    t = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);

	    if (abs((kdc_time & 0xffffffff) - t) > context->max_skew) {
		krb5_set_error_string(context, "time screw in server nonce ");
		ret = EINVAL;
		goto out;
	    }
	}

	if (strcasecmp(ireq.u.digestRequest.type, "CHAP") == 0) {
	    MD5_CTX ctx;
	    unsigned char md[MD5_DIGEST_LENGTH];
	    char *mdx;
	    char id;

	    if ((config->digests_allowed & CHAP_MD5) == 0) {
		kdc_log(context, config, 0, "Digest CHAP MD5 not allowed");
		goto out;
	    }

	    if (ireq.u.digestRequest.identifier == NULL) {
		krb5_set_error_string(context, "Identifier missing "
				      "from CHAP request");
		ret = EINVAL;
		goto out;
	    }
	    
	    if (hex_decode(*ireq.u.digestRequest.identifier, &id, 1) != 1) {
		krb5_set_error_string(context, "failed to decode identifier");
		ret = EINVAL;
		goto out;
	    }
	    
	    ret = get_password_entry(context, config, 
				     ireq.u.digestRequest.username,
				     &password);
	    if (ret)
		goto out;

	    MD5_Init(&ctx);
	    MD5_Update(&ctx, &id, 1);
	    MD5_Update(&ctx, password, strlen(password));
	    MD5_Update(&ctx, serverNonce.data, serverNonce.length);
	    MD5_Final(md, &ctx);

	    hex_encode(md, sizeof(md), &mdx);
	    if (mdx == NULL) {
		krb5_clear_error_string(context);
		ret = ENOMEM;
		goto out;
	    }

	    r.element = choice_DigestRepInner_response;

	    ret = strcasecmp(mdx, ireq.u.digestRequest.responseData);
	    free(mdx);
	    if (ret == 0) {
		r.u.response.success = TRUE;
	    } else {
		kdc_log(context, config, 0, 
			"CHAP reply mismatch for %s",
			ireq.u.digestRequest.username);
		r.u.response.success = FALSE;
	    }

	} else if (strcasecmp(ireq.u.digestRequest.type, "SASL-DIGEST-MD5") == 0) {
	    MD5_CTX ctx;
	    unsigned char md[MD5_DIGEST_LENGTH];
	    char *mdx;
	    char *A1, *A2;

	    if ((config->digests_allowed & DIGEST_MD5) == 0) {
		kdc_log(context, config, 0, "Digest SASL MD5 not allowed");
		goto out;
	    }

	    if (ireq.u.digestRequest.nonceCount == NULL) 
		goto out;
	    if (ireq.u.digestRequest.clientNonce == NULL) 
		goto out;
	    if (ireq.u.digestRequest.qop == NULL) 
		goto out;
	    if (ireq.u.digestRequest.realm == NULL) 
		goto out;
	    
	    ret = get_password_entry(context, config, 
				     ireq.u.digestRequest.username,
				     &password);
	    if (ret)
		goto failed;

	    MD5_Init(&ctx);
	    MD5_Update(&ctx, ireq.u.digestRequest.username,
		       strlen(ireq.u.digestRequest.username));
	    MD5_Update(&ctx, ":", 1);
	    MD5_Update(&ctx, *ireq.u.digestRequest.realm,
		       strlen(*ireq.u.digestRequest.realm));
	    MD5_Update(&ctx, ":", 1);
	    MD5_Update(&ctx, password, strlen(password));
	    MD5_Final(md, &ctx);
	    
	    MD5_Init(&ctx);
	    MD5_Update(&ctx, md, sizeof(md));
	    MD5_Update(&ctx, ":", 1);
	    MD5_Update(&ctx, ireq.u.digestRequest.serverNonce,
		       strlen(ireq.u.digestRequest.serverNonce));
	    MD5_Update(&ctx, ":", 1);
	    MD5_Update(&ctx, *ireq.u.digestRequest.nonceCount,
		       strlen(*ireq.u.digestRequest.nonceCount));
	    if (ireq.u.digestRequest.authid) {
		MD5_Update(&ctx, ":", 1);
		MD5_Update(&ctx, *ireq.u.digestRequest.authid,
			   strlen(*ireq.u.digestRequest.authid));
	    }
	    MD5_Final(md, &ctx);
	    hex_encode(md, sizeof(md), &A1);
	    if (A1 == NULL) {
		krb5_set_error_string(context, "out of memory");
		ret = ENOMEM;
		goto failed;
	    }
	    
	    MD5_Init(&ctx);
	    MD5_Update(&ctx, "AUTHENTICATE:", sizeof("AUTHENTICATE:") - 1);
	    MD5_Update(&ctx, *ireq.u.digestRequest.uri,
		       strlen(*ireq.u.digestRequest.uri));
	
	    /* conf|int */
	    if (strcmp(ireq.u.digestRequest.digest, "clear") != 0) {
		static char conf_zeros[] = ":00000000000000000000000000000000";
		MD5_Update(&ctx, conf_zeros, sizeof(conf_zeros) - 1);
	    }
	    
	    MD5_Final(md, &ctx);
	    hex_encode(md, sizeof(md), &A2);
	    if (A2 == NULL) {
		krb5_set_error_string(context, "out of memory");
		ret = ENOMEM;
		free(A1);
		goto failed;
	    }

	    MD5_Init(&ctx);
	    MD5_Update(&ctx, A1, strlen(A2));
	    MD5_Update(&ctx, ":", 1);
	    MD5_Update(&ctx, ireq.u.digestRequest.serverNonce,
		       strlen(ireq.u.digestRequest.serverNonce));
	    MD5_Update(&ctx, ":", 1);
	    MD5_Update(&ctx, *ireq.u.digestRequest.nonceCount,
		       strlen(*ireq.u.digestRequest.nonceCount));
	    MD5_Update(&ctx, ":", 1);
	    MD5_Update(&ctx, *ireq.u.digestRequest.clientNonce,
		       strlen(*ireq.u.digestRequest.clientNonce));
	    MD5_Update(&ctx, ":", 1);
	    MD5_Update(&ctx, *ireq.u.digestRequest.qop,
		       strlen(*ireq.u.digestRequest.qop));
	    MD5_Update(&ctx, ":", 1);
	    MD5_Update(&ctx, A2, strlen(A2));

	    MD5_Final(md, &ctx);

	    free(A1);
	    free(A2);

	    hex_encode(md, sizeof(md), &mdx);
	    if (mdx == NULL) {
		krb5_clear_error_string(context);
		ret = ENOMEM;
		goto out;
	    }

	    r.element = choice_DigestRepInner_response;
	    ret = strcasecmp(mdx, ireq.u.digestRequest.responseData);
	    free(mdx);
	    if (ret == 0) {
		r.u.response.success = TRUE;
	    } else {
		kdc_log(context, config, 0, 
			"DIGEST-MD5 reply mismatch for %s",
			ireq.u.digestRequest.username);
		r.u.response.success = FALSE;
	    }

	} else if (strcasecmp(ireq.u.digestRequest.type, "MS-CHAP-V2") == 0) {
	    unsigned char md[SHA_DIGEST_LENGTH], challange[SHA_DIGEST_LENGTH];
	    krb5_principal clientprincipal = NULL;
	    char *mdx;
	    const char *username;
	    struct ntlm_buf answer;
	    Key *key = NULL;
	    SHA_CTX ctx;

	    if ((config->digests_allowed & MS_CHAP_V2) == 0) {
		kdc_log(context, config, 0, "MS-CHAP-V2 not allowed");
		goto failed;
	    }

	    if (ireq.u.digestRequest.clientNonce == NULL)  {
		krb5_set_error_string(context, 
				      "MS-CHAP-V2 clientNonce missing");
		ret = EINVAL;
		goto failed;
	    }	    
	    if (serverNonce.length != 16) {
		krb5_set_error_string(context, 
				      "MS-CHAP-V2 serverNonce wrong length");
		ret = EINVAL;
		goto failed;
	    }

	    /* strip of the domain component */
	    username = strchr(ireq.u.digestRequest.username, '\\');
	    if (username == NULL)
		username = ireq.u.digestRequest.username;
	    else
		username++;

	    /* ChallangeHash */
	    SHA1_Init(&ctx);
	    {
		ssize_t ssize;
		krb5_data clientNonce;
		
		clientNonce.length = strlen(*ireq.u.digestRequest.clientNonce);
		clientNonce.data = malloc(clientNonce.length);
		if (clientNonce.data == NULL) {
		    ret = ENOMEM;
		    krb5_set_error_string(context, "out of memory");
		    goto out;
		}

		ssize = hex_decode(*ireq.u.digestRequest.clientNonce, 
				   clientNonce.data, clientNonce.length);
		if (ssize != 16) {
		    krb5_set_error_string(context, 
					  "Failed to decode clientNonce");
		    ret = ENOMEM;
		    goto out;
		}
		SHA1_Update(&ctx, clientNonce.data, ssize);
		free(clientNonce.data);
	    }
	    SHA1_Update(&ctx, serverNonce.data, serverNonce.length);
	    SHA1_Update(&ctx, username, strlen(username));
	    SHA1_Final(challange, &ctx);

	    /* NtPasswordHash */
	    ret = krb5_parse_name(context, username, &clientprincipal);
	    if (ret)
		goto failed;
	    
	    ret = _kdc_db_fetch(context, config, clientprincipal,
				HDB_F_GET_CLIENT, NULL, &user);
	    krb5_free_principal(context, clientprincipal);
	    if (ret) {
		krb5_set_error_string(context, 
				      "MS-CHAP-V2 user %s not in database",
				      username);
		goto failed;
	    }

	    ret = hdb_enctype2key(context, &user->entry, 
				  ETYPE_ARCFOUR_HMAC_MD5, &key);
	    if (ret) {
		krb5_set_error_string(context, 
				      "MS-CHAP-V2 missing arcfour key %s",
				      username);
		goto failed;
	    }

	    /* ChallengeResponse */
	    ret = heim_ntlm_calculate_ntlm1(key->key.keyvalue.data,
					    key->key.keyvalue.length,
					    challange, &answer);
	    if (ret) {
		krb5_set_error_string(context, "NTLM missing arcfour key");
		goto failed;
	    }
	    
	    hex_encode(answer.data, answer.length, &mdx);
	    if (mdx == NULL) {
		free(answer.data);
		krb5_clear_error_string(context);
		ret = ENOMEM;
		goto out;
	    }

	    r.element = choice_DigestRepInner_response;
	    ret = strcasecmp(mdx, ireq.u.digestRequest.responseData);
	    if (ret == 0) {
		r.u.response.success = TRUE;
	    } else {
		kdc_log(context, config, 0, 
			"MS-CHAP-V2 hash mismatch for %s",
			ireq.u.digestRequest.username);
		r.u.response.success = FALSE;
	    }
	    free(mdx);

	    if (r.u.response.success) {
		unsigned char hashhash[MD4_DIGEST_LENGTH];

		/* hashhash */
		{
		    MD4_CTX hctx;

		    MD4_Init(&hctx);
		    MD4_Update(&hctx, key->key.keyvalue.data, 
			       key->key.keyvalue.length);
		    MD4_Final(hashhash, &hctx);
		}

		/* GenerateAuthenticatorResponse */
		SHA1_Init(&ctx);
		SHA1_Update(&ctx, hashhash, sizeof(hashhash));
		SHA1_Update(&ctx, answer.data, answer.length);
		SHA1_Update(&ctx, ms_chap_v2_magic1,sizeof(ms_chap_v2_magic1));
		SHA1_Final(md, &ctx);

		SHA1_Init(&ctx);
		SHA1_Update(&ctx, md, sizeof(md));
		SHA1_Update(&ctx, challange, 8);
		SHA1_Update(&ctx, ms_chap_v2_magic2, sizeof(ms_chap_v2_magic2));
		SHA1_Final(md, &ctx);

		r.u.response.rsp = calloc(1, sizeof(*r.u.response.rsp));
		if (r.u.response.rsp == NULL) {
		    free(answer.data);
		    krb5_clear_error_string(context);
		    ret = ENOMEM;
		    goto out;
		}

		hex_encode(md, sizeof(md), r.u.response.rsp);
		if (r.u.response.rsp == NULL) {
		    free(answer.data);
		    krb5_clear_error_string(context);
		    ret = ENOMEM;
		    goto out;
		}

		/* get_master, rfc 3079 3.4 */
		SHA1_Init(&ctx);
		SHA1_Update(&ctx, hashhash, 16); /* md4(hash) */
		SHA1_Update(&ctx, answer.data, answer.length);
		SHA1_Update(&ctx, ms_rfc3079_magic1, sizeof(ms_rfc3079_magic1));
		SHA1_Final(md, &ctx);

		free(answer.data);

		r.u.response.session_key = 
		    calloc(1, sizeof(*r.u.response.session_key));
		if (r.u.response.session_key == NULL) {
		    krb5_clear_error_string(context);
		    ret = ENOMEM;
		    goto out;
		}

		ret = krb5_data_copy(r.u.response.session_key, md, 16);
		if (ret) {
		    krb5_clear_error_string(context);
		    goto out;
		}
	    }

	} else {
	    r.element = choice_DigestRepInner_error;
	    asprintf(&r.u.error.reason, "Unsupported digest type %s", 
		     ireq.u.digestRequest.type);
	    if (r.u.error.reason == NULL) {
		krb5_set_error_string(context, "out of memory");
		ret = ENOMEM;
		goto out;
	    }
	    r.u.error.code = EINVAL;
	}

	kdc_log(context, config, 0, "Digest %s request successful %s",
		ireq.u.digestRequest.type, ireq.u.digestRequest.username);

	break;
    }
    case choice_DigestReqInner_ntlmInit:

	if ((config->digests_allowed & (NTLM_V1|NTLM_V1_SESSION|NTLM_V2)) == 0) {
	    kdc_log(context, config, 0, "NTLM not allowed");
	    goto failed;
	}

	r.element = choice_DigestRepInner_ntlmInitReply;

	r.u.ntlmInitReply.flags = NTLM_NEG_UNICODE;

	if ((ireq.u.ntlmInit.flags & NTLM_NEG_UNICODE) == 0) {
	    kdc_log(context, config, 0, "NTLM client have no unicode");
	    goto failed;
	}

	if (ireq.u.ntlmInit.flags & NTLM_NEG_NTLM)
	    r.u.ntlmInitReply.flags |= NTLM_NEG_NTLM;
	else {
	    kdc_log(context, config, 0, "NTLM client doesn't support NTLM");
	    goto failed;
	}

	r.u.ntlmInitReply.flags |= 
	    NTLM_NEG_TARGET |
	    NTLM_TARGET_DOMAIN |
	    NTLM_ENC_128;

#define ALL					\
	NTLM_NEG_SIGN|				\
	    NTLM_NEG_SEAL|			\
	    NTLM_NEG_ALWAYS_SIGN|		\
	    NTLM_NEG_NTLM2_SESSION|		\
	    NTLM_NEG_KEYEX

	r.u.ntlmInitReply.flags |= (ireq.u.ntlmInit.flags & (ALL));

#undef ALL

	r.u.ntlmInitReply.targetname = 
	    get_ntlm_targetname(context, client);
	if (r.u.ntlmInitReply.targetname == NULL) {
	    krb5_set_error_string(context, "out of memory");
	    ret = ENOMEM;
	    goto out;
	}
	r.u.ntlmInitReply.challange.data = malloc(8);
	if (r.u.ntlmInitReply.challange.data == NULL) {
	    krb5_set_error_string(context, "out of memory");
	    ret = ENOMEM;
	    goto out;
	}
	r.u.ntlmInitReply.challange.length = 8;
	if (RAND_bytes(r.u.ntlmInitReply.challange.data,
		       r.u.ntlmInitReply.challange.length) != 1) 
	{
	    krb5_set_error_string(context, "out of random error");
	    ret = ENOMEM;
	    goto out;
	}
	/* XXX fix targetinfo */
	ALLOC(r.u.ntlmInitReply.targetinfo);
	if (r.u.ntlmInitReply.targetinfo == NULL) {
	    krb5_set_error_string(context, "out of memory");
	    ret = ENOMEM;
	    goto out;
	}

	ret = fill_targetinfo(context,
			      r.u.ntlmInitReply.targetname,
			      client,
			      r.u.ntlmInitReply.targetinfo);
	if (ret) {
	    krb5_set_error_string(context, "out of memory");
	    ret = ENOMEM;
	    goto out;
	}

	/* 
	 * Save data encryted in opaque for the second part of the
	 * ntlm authentication
	 */
	sp = krb5_storage_emem();
	if (sp == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_string(context, "out of memory");
	    goto out;
	}
	
	ret = krb5_storage_write(sp, r.u.ntlmInitReply.challange.data, 8);
	if (ret != 8) {
	    ret = ENOMEM;
	    krb5_set_error_string(context, "storage write challange");
	    goto out;
	}
	ret = krb5_store_uint32(sp, r.u.ntlmInitReply.flags);
	if (ret) {
	    krb5_clear_error_string(context);
	    goto out;
	}

	ret = krb5_storage_to_data(sp, &buf);
	if (ret) {
	    krb5_clear_error_string(context);
	    goto out;
	}

	ret = get_digest_key(context, config, server, &crypto);
	if (ret)
	    goto out;

	ret = krb5_encrypt(context, crypto, KRB5_KU_DIGEST_OPAQUE,
			   buf.data, buf.length, &r.u.ntlmInitReply.opaque);
	krb5_data_free(&buf);
	krb5_crypto_destroy(context, crypto);
	crypto = NULL;
	if (ret)
	    goto out;

	kdc_log(context, config, 0, "NTLM init from %s", from);

	break;

    case choice_DigestReqInner_ntlmRequest: {
	krb5_principal clientprincipal;
	unsigned char sessionkey[16];
	unsigned char challange[8];
	uint32_t flags;
	Key *key = NULL;
	int version;
	    
	r.element = choice_DigestRepInner_ntlmResponse;
	r.u.ntlmResponse.success = 0;
	r.u.ntlmResponse.flags = 0;
	r.u.ntlmResponse.sessionkey = NULL;
	r.u.ntlmResponse.tickets = NULL;

	/* get username */
	ret = krb5_parse_name(context,
			      ireq.u.ntlmRequest.username,
			      &clientprincipal);
	if (ret)
	    goto failed;

	ret = _kdc_db_fetch(context, config, clientprincipal,
			    HDB_F_GET_CLIENT, NULL, &user);
	krb5_free_principal(context, clientprincipal);
	if (ret) {
	    krb5_set_error_string(context, "NTLM user %s not in database",
				  ireq.u.ntlmRequest.username);
	    goto failed;
	}

	ret = get_digest_key(context, config, server, &crypto);
	if (ret)
	    goto failed;

	ret = krb5_decrypt(context, crypto, KRB5_KU_DIGEST_OPAQUE,
			   ireq.u.ntlmRequest.opaque.data,
			   ireq.u.ntlmRequest.opaque.length, &buf);
	krb5_crypto_destroy(context, crypto);
	crypto = NULL;
	if (ret) {
	    kdc_log(context, config, 0, 
		    "Failed to decrypt nonce from %s", from);
	    goto failed;
	}

	sp = krb5_storage_from_data(&buf);
	if (sp == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_string(context, "out of memory");
	    goto out;
	}
	
	ret = krb5_storage_read(sp, challange, sizeof(challange));
	if (ret != sizeof(challange)) {
	    krb5_set_error_string(context, "NTLM storage read challange");
	    ret = ENOMEM;
	    goto out;
	}
	ret = krb5_ret_uint32(sp, &flags);
	if (ret) {
	    krb5_set_error_string(context, "NTLM storage read flags");
	    goto out;
	}
	krb5_data_free(&buf);

	if ((flags & NTLM_NEG_NTLM) == 0) {
	    ret = EINVAL;
	    krb5_set_error_string(context, "NTLM not negotiated");
	    goto out;
	}

	ret = hdb_enctype2key(context, &user->entry, 
			      ETYPE_ARCFOUR_HMAC_MD5, &key);
	if (ret) {
	    krb5_set_error_string(context, "NTLM missing arcfour key");
	    goto out;
	}

	/* check if this is NTLMv2 */
	if (ireq.u.ntlmRequest.ntlm.length != 24) {
	    struct ntlm_buf infotarget, answer;
	    char *targetname;

	    if ((config->digests_allowed & NTLM_V2) == 0) {
		kdc_log(context, config, 0, "NTLM v2 not allowed");
		goto out;
	    }

	    version = 2;

	    targetname = get_ntlm_targetname(context, client);
	    if (targetname == NULL) {
		krb5_set_error_string(context, "out of memory");
		ret = ENOMEM;
		goto out;
	    }

	    answer.length = ireq.u.ntlmRequest.ntlm.length;
	    answer.data = ireq.u.ntlmRequest.ntlm.data;

	    ret = heim_ntlm_verify_ntlm2(key->key.keyvalue.data,
					 key->key.keyvalue.length,
					 ireq.u.ntlmRequest.username,
					 targetname,
					 0,
					 challange,
					 &answer,
					 &infotarget,
					 sessionkey);
	    free(targetname);
	    if (ret) {
		krb5_set_error_string(context, "NTLM v2 verify failed");
		goto failed;
	    }

	    /* XXX verify infotarget matches client (checksum ?) */

	    free(infotarget.data);
	    /* */

	} else {
	    struct ntlm_buf answer;

	    version = 1;

	    if (flags & NTLM_NEG_NTLM2_SESSION) {
		unsigned char sessionhash[MD5_DIGEST_LENGTH];
		MD5_CTX md5ctx;
		
		if ((config->digests_allowed & NTLM_V1_SESSION) == 0) {
		    kdc_log(context, config, 0, "NTLM v1-session not allowed");
		    ret = EINVAL;
		    goto failed;
		}

		if (ireq.u.ntlmRequest.lm.length != 24) {
		    krb5_set_error_string(context, "LM hash have wrong length "
					  "for NTLM session key");
		    ret = EINVAL;
		    goto failed;
		}
		
		MD5_Init(&md5ctx);
		MD5_Update(&md5ctx, challange, sizeof(challange));
		MD5_Update(&md5ctx, ireq.u.ntlmRequest.lm.data, 8);
		MD5_Final(sessionhash, &md5ctx);
		memcpy(challange, sessionhash, sizeof(challange));
	    } else {
		if ((config->digests_allowed & NTLM_V1) == 0) {
		    kdc_log(context, config, 0, "NTLM v1 not allowed");
		    goto failed;
		}
	    }
	    
	    ret = heim_ntlm_calculate_ntlm1(key->key.keyvalue.data,
					    key->key.keyvalue.length,
					    challange, &answer);
	    if (ret) {
		krb5_set_error_string(context, "NTLM missing arcfour key");
		goto failed;
	    }
	    
	    if (ireq.u.ntlmRequest.ntlm.length != answer.length ||
		memcmp(ireq.u.ntlmRequest.ntlm.data, answer.data, answer.length) != 0)
	    {
		free(answer.data);
		ret = EINVAL;
		krb5_set_error_string(context, "NTLM hash mismatch");
		goto failed;
	    }
	    free(answer.data);

	    {
		MD4_CTX ctx;

		MD4_Init(&ctx);
		MD4_Update(&ctx, 
			   key->key.keyvalue.data, key->key.keyvalue.length);
		MD4_Final(sessionkey, &ctx);
	    }
	}

	if (ireq.u.ntlmRequest.sessionkey) {
	    unsigned char masterkey[MD4_DIGEST_LENGTH];
	    RC4_KEY rc4;
	    size_t len;
	    
	    if ((flags & NTLM_NEG_KEYEX) == 0) {
		krb5_set_error_string(context,
				      "NTLM client failed to neg key "
				      "exchange but still sent key");
		ret = EINVAL;
		goto failed;
	    }
	    
	    len = ireq.u.ntlmRequest.sessionkey->length;
	    if (len != sizeof(masterkey)){
		krb5_set_error_string(context,
				      "NTLM master key wrong length: %lu",
				      (unsigned long)len);
		goto failed;
	    }
	    
	    RC4_set_key(&rc4, sizeof(sessionkey), sessionkey);
	    
	    RC4(&rc4, sizeof(masterkey),
		ireq.u.ntlmRequest.sessionkey->data, 
		masterkey);
	    memset(&rc4, 0, sizeof(rc4));
	    
	    r.u.ntlmResponse.sessionkey = 
		malloc(sizeof(*r.u.ntlmResponse.sessionkey));
	    if (r.u.ntlmResponse.sessionkey == NULL) {
		krb5_set_error_string(context, "out of memory");
		goto out;
	    }
	    
	    ret = krb5_data_copy(r.u.ntlmResponse.sessionkey,
				 masterkey, sizeof(masterkey));
	    if (ret) {
		krb5_set_error_string(context, "out of memory");
		goto out;
	    }
	}

	r.u.ntlmResponse.success = 1;
	kdc_log(context, config, 0, "NTLM version %d successful for %s",
		version, ireq.u.ntlmRequest.username);
	break;
    }
    case choice_DigestReqInner_supportedMechs:

	kdc_log(context, config, 0, "digest supportedMechs from %s", from);

	r.element = choice_DigestRepInner_supportedMechs;
	memset(&r.u.supportedMechs, 0, sizeof(r.u.supportedMechs));

	if (config->digests_allowed & NTLM_V1)
	    r.u.supportedMechs.ntlm_v1 = 1;
	if (config->digests_allowed & NTLM_V1_SESSION)
	    r.u.supportedMechs.ntlm_v1_session = 1;
	if (config->digests_allowed & NTLM_V2)
	    r.u.supportedMechs.ntlm_v2 = 1;
	if (config->digests_allowed & DIGEST_MD5)
	    r.u.supportedMechs.digest_md5 = 1;
	if (config->digests_allowed & CHAP_MD5)
	    r.u.supportedMechs.chap_md5 = 1;
	if (config->digests_allowed & MS_CHAP_V2)
	    r.u.supportedMechs.ms_chap_v2 = 1;
	break;

    default: {
	char *s;
	krb5_set_error_string(context, "unknown operation to digest");
	ret = EINVAL;

    failed:

	s = krb5_get_error_message(context, ret);
	if (s == NULL) {
	    krb5_clear_error_string(context);
	    goto out;
	}
	
	kdc_log(context, config, 0, "Digest failed with: %s", s);

	r.element = choice_DigestRepInner_error;
	r.u.error.reason = strdup("unknown error");
	krb5_free_error_string(context, s);
	if (r.u.error.reason == NULL) {
	    krb5_set_error_string(context, "out of memory");
	    ret = ENOMEM;
	    goto out;
	}
	r.u.error.code = EINVAL;
	break;
    }
    }

    ASN1_MALLOC_ENCODE(DigestRepInner, buf.data, buf.length, &r, &size, ret);
    if (ret) {
	krb5_set_error_string(context, "Failed to encode inner digest reply");
	goto out;
    }
    if (size != buf.length)
	krb5_abortx(context, "ASN1 internal error");

    krb5_auth_con_addflags(context, ac, KRB5_AUTH_CONTEXT_USE_SUBKEY, NULL);

    ret = krb5_mk_rep (context, ac, &rep.apRep);
    if (ret)
	goto out;

    {
	krb5_keyblock *key;

	ret = krb5_auth_con_getlocalsubkey(context, ac, &key);
	if (ret)
	    goto out;

	ret = krb5_crypto_init(context, key, 0, &crypto);
	krb5_free_keyblock (context, key);
	if (ret)
	    goto out;
    }

    ret = krb5_encrypt_EncryptedData(context, crypto, KRB5_KU_DIGEST_ENCRYPT, 
				     buf.data, buf.length, 0,
				     &rep.innerRep);
    
    ASN1_MALLOC_ENCODE(DigestREP, reply->data, reply->length, &rep, &size, ret);
    if (ret) {
	krb5_set_error_string(context, "Failed to encode digest reply");
	goto out;
    }
    if (size != reply->length)
	krb5_abortx(context, "ASN1 internal error");

    
out:
    if (ac)
	krb5_auth_con_free(context, ac);
    if (ret)
	krb5_warn(context, ret, "Digest request from %s failed", from);
    if (ticket)
	krb5_free_ticket(context, ticket);
    if (id)
	krb5_kt_close(context, id);
    if (crypto)
	krb5_crypto_destroy(context, crypto);
    if (sp)
	krb5_storage_free(sp);
    if (user)
	_kdc_free_ent (context, user);
    if (server)
	_kdc_free_ent (context, server);
    if (client)
	_kdc_free_ent (context, client);
    if (password) {
	memset(password, 0, strlen(password));
	free (password);
    }
    if (client_name)
	free (client_name);
    krb5_data_free(&buf);
    krb5_data_free(&serverNonce);
    free_DigestREP(&rep);
    free_DigestRepInner(&r);
    free_DigestReqInner(&ireq);

    return ret;
}
