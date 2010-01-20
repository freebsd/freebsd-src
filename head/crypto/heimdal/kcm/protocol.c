/*
 * Copyright (c) 2005, PADL Software Pty Ltd.
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
 * 3. Neither the name of PADL Software nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PADL SOFTWARE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PADL SOFTWARE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "kcm_locl.h"

RCSID("$Id: protocol.c 22112 2007-12-03 19:34:33Z lha $");

static krb5_error_code
kcm_op_noop(krb5_context context,
	    kcm_client *client,
	    kcm_operation opcode,
	    krb5_storage *request,
	    krb5_storage *response)
{
    KCM_LOG_REQUEST(context, client, opcode);

    return 0;	
}

/*
 * Request:
 *	NameZ
 * Response:
 *	NameZ
 *
 */
static krb5_error_code
kcm_op_get_name(krb5_context context,
		kcm_client *client,
		kcm_operation opcode,
		krb5_storage *request,
		krb5_storage *response)

{
    krb5_error_code ret;
    char *name = NULL;
    kcm_ccache ccache;

    ret = krb5_ret_stringz(request, &name);
    if (ret)
	return ret;

    KCM_LOG_REQUEST_NAME(context, client, opcode, name);

    ret = kcm_ccache_resolve_client(context, client, opcode,
				    name, &ccache);
    if (ret) {
	free(name);
	return ret;
    }

    ret = krb5_store_stringz(response, ccache->name);
    if (ret) {
	kcm_release_ccache(context, &ccache);
	free(name);
	return ret;
    }

    free(name);
    kcm_release_ccache(context, &ccache);
    return 0;
}

/*
 * Request:
 *	
 * Response:
 *	NameZ
 */
static krb5_error_code
kcm_op_gen_new(krb5_context context,
	       kcm_client *client,
	       kcm_operation opcode,
	       krb5_storage *request,
	       krb5_storage *response)
{
    krb5_error_code ret;
    char *name;

    KCM_LOG_REQUEST(context, client, opcode);

    name = kcm_ccache_nextid(client->pid, client->uid, client->gid);
    if (name == NULL) {
	return KRB5_CC_NOMEM;
    }

    ret = krb5_store_stringz(response, name);
    free(name);

    return ret;
}

/*
 * Request:
 *	NameZ
 *	Principal
 *	
 * Response:
 *	
 */
static krb5_error_code
kcm_op_initialize(krb5_context context,
		  kcm_client *client,
		  kcm_operation opcode,
		  krb5_storage *request,
		  krb5_storage *response)
{
    kcm_ccache ccache;
    krb5_principal principal;
    krb5_error_code ret;
    char *name;
#if 0
    kcm_event event;
#endif

    KCM_LOG_REQUEST(context, client, opcode);

    ret = krb5_ret_stringz(request, &name);
    if (ret)
	return ret;

    ret = krb5_ret_principal(request, &principal);
    if (ret) {
	free(name);
	return ret;
    }

    ret = kcm_ccache_new_client(context, client, name, &ccache);
    if (ret) {
	free(name);
	krb5_free_principal(context, principal);
	return ret;
    }

    ccache->client = principal;

    free(name);

#if 0
    /*
     * Create a new credentials cache. To mitigate DoS attacks we will
     * expire it in 30 minutes unless it has some credentials added
     * to it
     */

    event.fire_time = 30 * 60;
    event.expire_time = 0;
    event.backoff_time = 0;
    event.action = KCM_EVENT_DESTROY_EMPTY_CACHE;
    event.ccache = ccache;

    ret = kcm_enqueue_event_relative(context, &event);
#endif

    kcm_release_ccache(context, &ccache);

    return ret;
}

/*
 * Request:
 *	NameZ
 *	
 * Response:
 *	
 */
static krb5_error_code
kcm_op_destroy(krb5_context context,
	       kcm_client *client,
	       kcm_operation opcode,
	       krb5_storage *request,
	       krb5_storage *response)
{
    krb5_error_code ret;
    char *name;

    ret = krb5_ret_stringz(request, &name);
    if (ret)
	return ret;

    KCM_LOG_REQUEST_NAME(context, client, opcode, name);

    ret = kcm_ccache_destroy_client(context, client, name);

    free(name);

    return ret;
}

/*
 * Request:
 *	NameZ
 *	Creds
 *	
 * Response:
 *	
 */
static krb5_error_code
kcm_op_store(krb5_context context,
	     kcm_client *client,
	     kcm_operation opcode,
	     krb5_storage *request,
	     krb5_storage *response)
{
    krb5_creds creds;
    krb5_error_code ret;
    kcm_ccache ccache;
    char *name;

    ret = krb5_ret_stringz(request, &name);
    if (ret)
	return ret;

    KCM_LOG_REQUEST_NAME(context, client, opcode, name);

    ret = krb5_ret_creds(request, &creds);
    if (ret) {
	free(name);
	return ret;
    }

    ret = kcm_ccache_resolve_client(context, client, opcode,
				    name, &ccache);
    if (ret) {
	free(name);
	krb5_free_cred_contents(context, &creds);
	return ret;
    }

    ret = kcm_ccache_store_cred(context, ccache, &creds, 0);
    if (ret) {
	free(name);
	krb5_free_cred_contents(context, &creds);
	kcm_release_ccache(context, &ccache);
	return ret;
    }

    kcm_ccache_enqueue_default(context, ccache, &creds);

    free(name);
    kcm_release_ccache(context, &ccache);

    return 0;
}

/*
 * Request:
 *	NameZ
 *	WhichFields
 *	MatchCreds
 *
 * Response:
 *	Creds
 *	
 */
static krb5_error_code
kcm_op_retrieve(krb5_context context,
		kcm_client *client,
		kcm_operation opcode,
		krb5_storage *request,
		krb5_storage *response)
{
    uint32_t flags;
    krb5_creds mcreds;
    krb5_error_code ret;
    kcm_ccache ccache;
    char *name;
    krb5_creds *credp;
    int free_creds = 0;

    ret = krb5_ret_stringz(request, &name);
    if (ret)
	return ret;

    KCM_LOG_REQUEST_NAME(context, client, opcode, name);

    ret = krb5_ret_uint32(request, &flags);
    if (ret) {
	free(name);
	return ret;
    }

    ret = krb5_ret_creds_tag(request, &mcreds);
    if (ret) {
	free(name);
	return ret;
    }

    if (disallow_getting_krbtgt &&
	mcreds.server->name.name_string.len == 2 &&
	strcmp(mcreds.server->name.name_string.val[0], KRB5_TGS_NAME) == 0)
    {
	free(name);
	krb5_free_cred_contents(context, &mcreds);
	return KRB5_FCC_PERM;
    }

    ret = kcm_ccache_resolve_client(context, client, opcode,
				    name, &ccache);
    if (ret) {
	free(name);
	krb5_free_cred_contents(context, &mcreds);
	return ret;
    }

    ret = kcm_ccache_retrieve_cred(context, ccache, flags,
				   &mcreds, &credp);
    if (ret && ((flags & KRB5_GC_CACHED) == 0)) {
	krb5_ccache_data ccdata;

	/* try and acquire */
	HEIMDAL_MUTEX_lock(&ccache->mutex);

	/* Fake up an internal ccache */
	kcm_internal_ccache(context, ccache, &ccdata);

	/* glue cc layer will store creds */
	ret = krb5_get_credentials(context, 0, &ccdata, &mcreds, &credp);
	if (ret == 0)
	    free_creds = 1;

	HEIMDAL_MUTEX_unlock(&ccache->mutex);
    }

    if (ret == 0) {
	ret = krb5_store_creds(response, credp);
    }

    free(name);
    krb5_free_cred_contents(context, &mcreds);
    kcm_release_ccache(context, &ccache);

    if (free_creds)
	krb5_free_cred_contents(context, credp);

    return ret;
}

/*
 * Request:
 *	NameZ
 *
 * Response:
 *	Principal
 */
static krb5_error_code
kcm_op_get_principal(krb5_context context,
		     kcm_client *client,
		     kcm_operation opcode,
		     krb5_storage *request,
		     krb5_storage *response)
{
    krb5_error_code ret;
    kcm_ccache ccache;
    char *name;

    ret = krb5_ret_stringz(request, &name);
    if (ret)
	return ret;

    KCM_LOG_REQUEST_NAME(context, client, opcode, name);

    ret = kcm_ccache_resolve_client(context, client, opcode,
				    name, &ccache);
    if (ret) {
	free(name);
	return ret;
    }

    if (ccache->client == NULL)
	ret = KRB5_CC_NOTFOUND;
    else
	ret = krb5_store_principal(response, ccache->client);

    free(name);
    kcm_release_ccache(context, &ccache);

    return 0;
}

/*
 * Request:
 *	NameZ
 *
 * Response:
 *	Cursor
 *	
 */
static krb5_error_code
kcm_op_get_first(krb5_context context,
		 kcm_client *client,
		 kcm_operation opcode,
		 krb5_storage *request,
		 krb5_storage *response)
{
    krb5_error_code ret;
    kcm_ccache ccache;
    uint32_t cursor;
    char *name;

    ret = krb5_ret_stringz(request, &name);
    if (ret)
	return ret;

    KCM_LOG_REQUEST_NAME(context, client, opcode, name);

    ret = kcm_ccache_resolve_client(context, client, opcode,
				    name, &ccache);
    if (ret) {
	free(name);
	return ret;
    }

    ret = kcm_cursor_new(context, client->pid, ccache, &cursor);
    if (ret) {
	kcm_release_ccache(context, &ccache);
	free(name);
	return ret;
    }

    ret = krb5_store_int32(response, cursor);

    free(name);
    kcm_release_ccache(context, &ccache);

    return ret;
}

/*
 * Request:
 *	NameZ
 *	Cursor
 *
 * Response:
 *	Creds
 */
static krb5_error_code
kcm_op_get_next(krb5_context context,
		kcm_client *client,
		kcm_operation opcode,
		krb5_storage *request,
		krb5_storage *response)
{
    krb5_error_code ret;
    kcm_ccache ccache;
    char *name;
    uint32_t cursor;
    kcm_cursor *c;

    ret = krb5_ret_stringz(request, &name);
    if (ret)
	return ret;

    KCM_LOG_REQUEST_NAME(context, client, opcode, name);

    ret = krb5_ret_uint32(request, &cursor);
    if (ret) {
	free(name);
	return ret;
    }

    ret = kcm_ccache_resolve_client(context, client, opcode,
				    name, &ccache);
    if (ret) {
	free(name);
	return ret;
    }

    ret = kcm_cursor_find(context, client->pid, ccache, cursor, &c);
    if (ret) {
	kcm_release_ccache(context, &ccache);
	free(name);
	return ret;
    }

    HEIMDAL_MUTEX_lock(&ccache->mutex);
    if (c->credp == NULL) {
	ret = KRB5_CC_END;
    } else {
	ret = krb5_store_creds(response, &c->credp->cred);
	c->credp = c->credp->next;
    }
    HEIMDAL_MUTEX_unlock(&ccache->mutex);

    free(name);
    kcm_release_ccache(context, &ccache);

    return ret;
}

/*
 * Request:
 *	NameZ
 *	Cursor
 *
 * Response:
 *	
 */
static krb5_error_code
kcm_op_end_get(krb5_context context,
	       kcm_client *client,
	       kcm_operation opcode,
	       krb5_storage *request,
	       krb5_storage *response)
{
    krb5_error_code ret;
    kcm_ccache ccache;
    uint32_t cursor;
    char *name;

    ret = krb5_ret_stringz(request, &name);
    if (ret)
	return ret;

    KCM_LOG_REQUEST_NAME(context, client, opcode, name);

    ret = krb5_ret_uint32(request, &cursor);
    if (ret) {
	free(name);
	return ret;
    }

    ret = kcm_ccache_resolve_client(context, client, opcode,
				    name, &ccache);
    if (ret) {
	free(name);
	return ret;
    }

    ret = kcm_cursor_delete(context, client->pid, ccache, cursor);

    free(name);
    kcm_release_ccache(context, &ccache);

    return ret;
}

/*
 * Request:
 *	NameZ
 *	WhichFields
 *	MatchCreds
 *
 * Response:
 *	
 */
static krb5_error_code
kcm_op_remove_cred(krb5_context context,
		   kcm_client *client,
		   kcm_operation opcode,
		   krb5_storage *request,
		   krb5_storage *response)
{
    uint32_t whichfields;
    krb5_creds mcreds;
    krb5_error_code ret;
    kcm_ccache ccache;
    char *name;

    ret = krb5_ret_stringz(request, &name);
    if (ret)
	return ret;

    KCM_LOG_REQUEST_NAME(context, client, opcode, name);

    ret = krb5_ret_uint32(request, &whichfields);
    if (ret) {
	free(name);
	return ret;
    }

    ret = krb5_ret_creds_tag(request, &mcreds);
    if (ret) {
	free(name);
	return ret;
    }

    ret = kcm_ccache_resolve_client(context, client, opcode,
				    name, &ccache);
    if (ret) {
	free(name);
	krb5_free_cred_contents(context, &mcreds);
	return ret;
    }

    ret = kcm_ccache_remove_cred(context, ccache, whichfields, &mcreds);

    /* XXX need to remove any events that match */

    free(name);
    krb5_free_cred_contents(context, &mcreds);
    kcm_release_ccache(context, &ccache);

    return ret;
}

/*
 * Request:
 *	NameZ
 *	Flags
 *
 * Response:
 *	
 */
static krb5_error_code
kcm_op_set_flags(krb5_context context,
		 kcm_client *client,
		 kcm_operation opcode,
		 krb5_storage *request,
		 krb5_storage *response)
{
    uint32_t flags;
    krb5_error_code ret;
    kcm_ccache ccache;
    char *name;

    ret = krb5_ret_stringz(request, &name);
    if (ret)
	return ret;

    KCM_LOG_REQUEST_NAME(context, client, opcode, name);

    ret = krb5_ret_uint32(request, &flags);
    if (ret) {
	free(name);
	return ret;
    }

    ret = kcm_ccache_resolve_client(context, client, opcode,
				    name, &ccache);
    if (ret) {
	free(name);
	return ret;
    }

    /* we don't really support any flags yet */
    free(name);
    kcm_release_ccache(context, &ccache);

    return 0;
}

/*
 * Request:
 *	NameZ
 *	UID
 *	GID
 *
 * Response:
 *	
 */
static krb5_error_code
kcm_op_chown(krb5_context context,
	     kcm_client *client,
	     kcm_operation opcode,
	     krb5_storage *request,
	     krb5_storage *response)
{
    uint32_t uid;
    uint32_t gid;
    krb5_error_code ret;
    kcm_ccache ccache;
    char *name;

    ret = krb5_ret_stringz(request, &name);
    if (ret)
	return ret;

    KCM_LOG_REQUEST_NAME(context, client, opcode, name);

    ret = krb5_ret_uint32(request, &uid);
    if (ret) {
	free(name);
	return ret;
    }

    ret = krb5_ret_uint32(request, &gid);
    if (ret) {
	free(name);
	return ret;
    }

    ret = kcm_ccache_resolve_client(context, client, opcode,
				    name, &ccache);
    if (ret) {
	free(name);
	return ret;
    }

    ret = kcm_chown(context, client, ccache, uid, gid);

    free(name);
    kcm_release_ccache(context, &ccache);

    return ret;
}

/*
 * Request:
 *	NameZ
 *	Mode
 *
 * Response:
 *	
 */
static krb5_error_code
kcm_op_chmod(krb5_context context,
	     kcm_client *client,
	     kcm_operation opcode,
	     krb5_storage *request,
	     krb5_storage *response)
{
    uint16_t mode;
    krb5_error_code ret;
    kcm_ccache ccache;
    char *name;

    ret = krb5_ret_stringz(request, &name);
    if (ret)
	return ret;

    KCM_LOG_REQUEST_NAME(context, client, opcode, name);

    ret = krb5_ret_uint16(request, &mode);
    if (ret) {
	free(name);
	return ret;
    }

    ret = kcm_ccache_resolve_client(context, client, opcode,
				    name, &ccache);
    if (ret) {
	free(name);
	return ret;
    }

    ret = kcm_chmod(context, client, ccache, mode);

    free(name);
    kcm_release_ccache(context, &ccache);

    return ret;
}

/*
 * Protocol extensions for moving ticket acquisition responsibility
 * from client to KCM follow.
 */

/*
 * Request:
 *	NameZ
 *	ServerPrincipalPresent
 *	ServerPrincipal OPTIONAL
 *	Key
 *
 * Repsonse:
 *
 */
static krb5_error_code
kcm_op_get_initial_ticket(krb5_context context,
			  kcm_client *client,
			  kcm_operation opcode,
			  krb5_storage *request,
			  krb5_storage *response)
{
    krb5_error_code ret;
    kcm_ccache ccache;
    char *name;
    int8_t not_tgt = 0;
    krb5_principal server = NULL;
    krb5_keyblock key;

    krb5_keyblock_zero(&key);

    ret = krb5_ret_stringz(request, &name);
    if (ret)
	return ret;

    KCM_LOG_REQUEST_NAME(context, client, opcode, name);

    ret = krb5_ret_int8(request, &not_tgt);
    if (ret) {
	free(name);
	return ret;
    }

    if (not_tgt) {
	ret = krb5_ret_principal(request, &server);
	if (ret) {
	    free(name);
	    return ret;
	}
    }

    ret = krb5_ret_keyblock(request, &key);
    if (ret) {
	free(name);
	if (server != NULL)
	    krb5_free_principal(context, server);
	return ret;
    }

    ret = kcm_ccache_resolve_client(context, client, opcode,
				    name, &ccache);
    if (ret == 0) {
	HEIMDAL_MUTEX_lock(&ccache->mutex);

	if (ccache->server != NULL) {
	    krb5_free_principal(context, ccache->server);
	    ccache->server = NULL;
	}

	krb5_free_keyblock(context, &ccache->key.keyblock);

	ccache->server = server;
	ccache->key.keyblock = key;
    	ccache->flags |= KCM_FLAGS_USE_CACHED_KEY;

	ret = kcm_ccache_enqueue_default(context, ccache, NULL);
	if (ret) {
	    ccache->server = NULL;
	    krb5_keyblock_zero(&ccache->key.keyblock);
	    ccache->flags &= ~(KCM_FLAGS_USE_CACHED_KEY);
	}

	HEIMDAL_MUTEX_unlock(&ccache->mutex);
    }

    free(name);

    if (ret != 0) {
	krb5_free_principal(context, server);
	krb5_free_keyblock(context, &key);
    }

    kcm_release_ccache(context, &ccache);

    return ret;
}

/*
 * Request:
 *	NameZ
 *	ServerPrincipal
 *	KDCFlags
 *	EncryptionType
 *
 * Repsonse:
 *
 */
static krb5_error_code
kcm_op_get_ticket(krb5_context context,
		  kcm_client *client,
		  kcm_operation opcode,
		  krb5_storage *request,
		  krb5_storage *response)
{
    krb5_error_code ret;
    kcm_ccache ccache;
    char *name;
    krb5_principal server = NULL;
    krb5_ccache_data ccdata;
    krb5_creds in, *out;
    krb5_kdc_flags flags;

    memset(&in, 0, sizeof(in));

    ret = krb5_ret_stringz(request, &name);
    if (ret)
	return ret;

    KCM_LOG_REQUEST_NAME(context, client, opcode, name);

    ret = krb5_ret_uint32(request, &flags.i);
    if (ret) {
	free(name);
	return ret;
    }

    ret = krb5_ret_int32(request, &in.session.keytype);
    if (ret) {
	free(name);
	return ret;
    }

    ret = krb5_ret_principal(request, &server);
    if (ret) {
	free(name);
	return ret;
    }

    ret = kcm_ccache_resolve_client(context, client, opcode,
				    name, &ccache);
    if (ret) {
	krb5_free_principal(context, server);
	free(name);
	return ret;
    }
 
    HEIMDAL_MUTEX_lock(&ccache->mutex);

    /* Fake up an internal ccache */
    kcm_internal_ccache(context, ccache, &ccdata);

    in.client = ccache->client;
    in.server = server;
    in.times.endtime = 0;

    /* glue cc layer will store creds */
    ret = krb5_get_credentials_with_flags(context, 0, flags,
					  &ccdata, &in, &out);

    HEIMDAL_MUTEX_unlock(&ccache->mutex);

    if (ret == 0)
	krb5_free_cred_contents(context, out);

    free(name);

    return ret;
}

static struct kcm_op kcm_ops[] = {
    { "NOOP", 			kcm_op_noop },
    { "GET_NAME",		kcm_op_get_name },
    { "RESOLVE",		kcm_op_noop },
    { "GEN_NEW", 		kcm_op_gen_new },
    { "INITIALIZE",		kcm_op_initialize },
    { "DESTROY",		kcm_op_destroy },
    { "STORE",			kcm_op_store },
    { "RETRIEVE",		kcm_op_retrieve },
    { "GET_PRINCIPAL",		kcm_op_get_principal },
    { "GET_FIRST",		kcm_op_get_first },
    { "GET_NEXT",		kcm_op_get_next },
    { "END_GET",		kcm_op_end_get },
    { "REMOVE_CRED",		kcm_op_remove_cred },
    { "SET_FLAGS",		kcm_op_set_flags },
    { "CHOWN",			kcm_op_chown },
    { "CHMOD",			kcm_op_chmod },
    { "GET_INITIAL_TICKET",	kcm_op_get_initial_ticket },
    { "GET_TICKET",		kcm_op_get_ticket }
};


const char *kcm_op2string(kcm_operation opcode)
{
    if (opcode >= sizeof(kcm_ops)/sizeof(kcm_ops[0]))
	return "Unknown operation";

    return kcm_ops[opcode].name;
}

krb5_error_code
kcm_dispatch(krb5_context context,
	     kcm_client *client,
	     krb5_data *req_data,
	     krb5_data *resp_data)
{
    krb5_error_code ret;
    kcm_method method;
    krb5_storage *req_sp = NULL;
    krb5_storage *resp_sp = NULL;
    uint16_t opcode;

    resp_sp = krb5_storage_emem();
    if (resp_sp == NULL) {
	return ENOMEM;
    }

    if (client->pid == -1) {
	kcm_log(0, "Client had invalid process number");
	ret = KRB5_FCC_INTERNAL;
	goto out;
    }

    req_sp = krb5_storage_from_data(req_data);
    if (req_sp == NULL) {
	kcm_log(0, "Process %d: failed to initialize storage from data",
		client->pid);
	ret = KRB5_CC_IO;
	goto out;
    }

    ret = krb5_ret_uint16(req_sp, &opcode);
    if (ret) {
	kcm_log(0, "Process %d: didn't send a message", client->pid);
	goto out;
    }

    if (opcode >= sizeof(kcm_ops)/sizeof(kcm_ops[0])) {
	kcm_log(0, "Process %d: invalid operation code %d",
		client->pid, opcode);
	ret = KRB5_FCC_INTERNAL;
	goto out;
    }
    method = kcm_ops[opcode].method;

    /* seek past place for status code */
    krb5_storage_seek(resp_sp, 4, SEEK_SET);

    ret = (*method)(context, client, opcode, req_sp, resp_sp);

out:
    if (req_sp != NULL) {
	krb5_storage_free(req_sp);
    }

    krb5_storage_seek(resp_sp, 0, SEEK_SET);
    krb5_store_int32(resp_sp, ret);

    ret = krb5_storage_to_data(resp_sp, resp_data);
    krb5_storage_free(resp_sp);

    return ret;
}

