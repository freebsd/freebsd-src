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

#include "krb5_locl.h"

#ifdef HAVE_KCM
/*
 * Client library for Kerberos Credentials Manager (KCM) daemon
 */

#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#include "kcm.h"

RCSID("$Id: kcm.c 22108 2007-12-03 17:23:53Z lha $");

typedef struct krb5_kcmcache {
    char *name;
    struct sockaddr_un path;
    char *door_path;
} krb5_kcmcache;

#define KCMCACHE(X)	((krb5_kcmcache *)(X)->data.data)
#define CACHENAME(X)	(KCMCACHE(X)->name)
#define KCMCURSOR(C)	(*(uint32_t *)(C))

static krb5_error_code
try_door(krb5_context context, const krb5_kcmcache *k,
	 krb5_data *request_data,
	 krb5_data *response_data)
{
#ifdef HAVE_DOOR_CREATE
    door_arg_t arg;
    int fd;
    int ret;

    memset(&arg, 0, sizeof(arg));
	   
    fd = open(k->door_path, O_RDWR);
    if (fd < 0)
	return KRB5_CC_IO;

    arg.data_ptr = request_data->data;
    arg.data_size = request_data->length;
    arg.desc_ptr = NULL;
    arg.desc_num = 0;
    arg.rbuf = NULL;
    arg.rsize = 0;

    ret = door_call(fd, &arg);
    close(fd);
    if (ret != 0)
	return KRB5_CC_IO;

    ret = krb5_data_copy(response_data, arg.rbuf, arg.rsize);
    munmap(arg.rbuf, arg.rsize);
    if (ret)
	return ret;

    return 0;
#else
    return KRB5_CC_IO;
#endif
}

static krb5_error_code
try_unix_socket(krb5_context context, const krb5_kcmcache *k,
		krb5_data *request_data,
		krb5_data *response_data)
{
    krb5_error_code ret;
    int fd;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
	return KRB5_CC_IO;
    
    if (connect(fd, rk_UNCONST(&k->path), sizeof(k->path)) != 0) {
	close(fd);
	return KRB5_CC_IO;
    }
    
    ret = _krb5_send_and_recv_tcp(fd, context->kdc_timeout,
				  request_data, response_data);
    close(fd);
    return ret;
}
    
static krb5_error_code
kcm_send_request(krb5_context context,
		 krb5_kcmcache *k,
		 krb5_storage *request,
		 krb5_data *response_data)
{
    krb5_error_code ret;
    krb5_data request_data;
    int i;

    response_data->data = NULL;
    response_data->length = 0;

    ret = krb5_storage_to_data(request, &request_data);
    if (ret) {
	krb5_clear_error_string(context);
	return KRB5_CC_NOMEM;
    }

    ret = KRB5_CC_IO;

    for (i = 0; i < context->max_retries; i++) {
	ret = try_door(context, k, &request_data, response_data);
	if (ret == 0 && response_data->length != 0)
	    break;
	ret = try_unix_socket(context, k, &request_data, response_data);
	if (ret == 0 && response_data->length != 0)
	    break;
    }

    krb5_data_free(&request_data);

    if (ret) {
	krb5_clear_error_string(context);
	ret = KRB5_CC_IO;
    }

    return ret;
}

static krb5_error_code
kcm_storage_request(krb5_context context,
		    kcm_operation opcode,
		    krb5_storage **storage_p)
{
    krb5_storage *sp;
    krb5_error_code ret;

    *storage_p = NULL;

    sp = krb5_storage_emem();
    if (sp == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return KRB5_CC_NOMEM;
    }

    /* Send MAJOR | VERSION | OPCODE */
    ret  = krb5_store_int8(sp, KCM_PROTOCOL_VERSION_MAJOR);
    if (ret)
	goto fail;
    ret = krb5_store_int8(sp, KCM_PROTOCOL_VERSION_MINOR);
    if (ret)
	goto fail;
    ret = krb5_store_int16(sp, opcode);
    if (ret)
	goto fail;

    *storage_p = sp;
 fail:
    if (ret) {
	krb5_set_error_string(context, "Failed to encode request");
	krb5_storage_free(sp);
    }
   
    return ret; 
}

static krb5_error_code
kcm_alloc(krb5_context context, const char *name, krb5_ccache *id)
{
    krb5_kcmcache *k;
    const char *path;

    k = malloc(sizeof(*k));
    if (k == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return KRB5_CC_NOMEM;
    }

    if (name != NULL) {
	k->name = strdup(name);
	if (k->name == NULL) {
	    free(k);
	    krb5_set_error_string(context, "malloc: out of memory");
	    return KRB5_CC_NOMEM;
	}
    } else
	k->name = NULL;

    path = krb5_config_get_string_default(context, NULL,
					  _PATH_KCM_SOCKET,
					  "libdefaults", 
					  "kcm_socket",
					  NULL);
    
    k->path.sun_family = AF_UNIX;
    strlcpy(k->path.sun_path, path, sizeof(k->path.sun_path));

    path = krb5_config_get_string_default(context, NULL,
					  _PATH_KCM_DOOR,
					  "libdefaults", 
					  "kcm_door",
					  NULL);
    k->door_path = strdup(path);

    (*id)->data.data = k;
    (*id)->data.length = sizeof(*k);

    return 0;
}

static krb5_error_code
kcm_call(krb5_context context,
	 krb5_kcmcache *k,
	 krb5_storage *request,
	 krb5_storage **response_p,
	 krb5_data *response_data_p)
{
    krb5_data response_data;
    krb5_error_code ret;
    int32_t status;
    krb5_storage *response;

    if (response_p != NULL)
	*response_p = NULL;

    ret = kcm_send_request(context, k, request, &response_data);
    if (ret) {
	return ret;
    }

    response = krb5_storage_from_data(&response_data);
    if (response == NULL) {
	krb5_data_free(&response_data);
	return KRB5_CC_IO;
    }

    ret = krb5_ret_int32(response, &status);
    if (ret) {
	krb5_storage_free(response);
	krb5_data_free(&response_data);
	return KRB5_CC_FORMAT;
    }

    if (status) {
	krb5_storage_free(response);
	krb5_data_free(&response_data);
	return status;
    }

    if (response_p != NULL) {
	*response_data_p = response_data;
	*response_p = response;

	return 0;
    }

    krb5_storage_free(response);
    krb5_data_free(&response_data);

    return 0;
}

static void
kcm_free(krb5_context context, krb5_ccache *id)
{
    krb5_kcmcache *k = KCMCACHE(*id);

    if (k != NULL) {
	if (k->name != NULL)
	    free(k->name);
	if (k->door_path)
	    free(k->door_path);
	memset(k, 0, sizeof(*k));
	krb5_data_free(&(*id)->data);
    }

    *id = NULL;
}

static const char *
kcm_get_name(krb5_context context,
	     krb5_ccache id)
{
    return CACHENAME(id);
}

static krb5_error_code
kcm_resolve(krb5_context context, krb5_ccache *id, const char *res)
{
    return kcm_alloc(context, res, id);
}

/*
 * Request:
 *
 * Response:
 *      NameZ
 */
static krb5_error_code
kcm_gen_new(krb5_context context, krb5_ccache *id)
{
    krb5_kcmcache *k;
    krb5_error_code ret;
    krb5_storage *request, *response;
    krb5_data response_data;

    ret = kcm_alloc(context, NULL, id);
    if (ret)
	return ret;

    k = KCMCACHE(*id);

    ret = kcm_storage_request(context, KCM_OP_GEN_NEW, &request);
    if (ret) {
	kcm_free(context, id);
	return ret;
    }

    ret = kcm_call(context, k, request, &response, &response_data);
    if (ret) {
	krb5_storage_free(request);
	kcm_free(context, id);
	return ret;
    }

    ret = krb5_ret_stringz(response, &k->name);
    if (ret)
	ret = KRB5_CC_IO;

    krb5_storage_free(request);
    krb5_storage_free(response);
    krb5_data_free(&response_data);

    if (ret)
	kcm_free(context, id);

    return ret;
}

/*
 * Request:
 *      NameZ
 *      Principal
 *
 * Response:
 *
 */
static krb5_error_code
kcm_initialize(krb5_context context,
	       krb5_ccache id,
	       krb5_principal primary_principal)
{
    krb5_error_code ret;
    krb5_kcmcache *k = KCMCACHE(id);
    krb5_storage *request;

    ret = kcm_storage_request(context, KCM_OP_INITIALIZE, &request);
    if (ret)
	return ret;

    ret = krb5_store_stringz(request, k->name);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_store_principal(request, primary_principal);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = kcm_call(context, k, request, NULL, NULL);

    krb5_storage_free(request);
    return ret;
}

static krb5_error_code
kcm_close(krb5_context context,
	  krb5_ccache id)
{
    kcm_free(context, &id);
    return 0;
}

/*
 * Request:
 *      NameZ
 *
 * Response:
 *
 */
static krb5_error_code
kcm_destroy(krb5_context context,
	    krb5_ccache id)
{
    krb5_error_code ret;
    krb5_kcmcache *k = KCMCACHE(id);
    krb5_storage *request;

    ret = kcm_storage_request(context, KCM_OP_DESTROY, &request);
    if (ret)
	return ret;

    ret = krb5_store_stringz(request, k->name);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = kcm_call(context, k, request, NULL, NULL);

    krb5_storage_free(request);
    return ret;
}

/*
 * Request:
 *      NameZ
 *      Creds
 *
 * Response:
 *
 */
static krb5_error_code
kcm_store_cred(krb5_context context,
	       krb5_ccache id,
	       krb5_creds *creds)
{
    krb5_error_code ret;
    krb5_kcmcache *k = KCMCACHE(id);
    krb5_storage *request;

    ret = kcm_storage_request(context, KCM_OP_STORE, &request);
    if (ret)
	return ret;

    ret = krb5_store_stringz(request, k->name);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_store_creds(request, creds);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = kcm_call(context, k, request, NULL, NULL);

    krb5_storage_free(request);
    return ret;
}

/*
 * Request:
 *      NameZ
 *      WhichFields
 *      MatchCreds
 *
 * Response:
 *      Creds
 *
 */
static krb5_error_code
kcm_retrieve(krb5_context context,
	     krb5_ccache id,
	     krb5_flags which,
	     const krb5_creds *mcred,
	     krb5_creds *creds)
{
    krb5_error_code ret;
    krb5_kcmcache *k = KCMCACHE(id);
    krb5_storage *request, *response;
    krb5_data response_data;

    ret = kcm_storage_request(context, KCM_OP_RETRIEVE, &request);
    if (ret)
	return ret;

    ret = krb5_store_stringz(request, k->name);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_store_int32(request, which);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_store_creds_tag(request, rk_UNCONST(mcred));
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = kcm_call(context, k, request, &response, &response_data);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_ret_creds(response, creds);
    if (ret)
	ret = KRB5_CC_IO;

    krb5_storage_free(request);
    krb5_storage_free(response);
    krb5_data_free(&response_data);

    return ret;
}

/*
 * Request:
 *      NameZ
 *
 * Response:
 *      Principal
 */
static krb5_error_code
kcm_get_principal(krb5_context context,
		  krb5_ccache id,
		  krb5_principal *principal)
{
    krb5_error_code ret;
    krb5_kcmcache *k = KCMCACHE(id);
    krb5_storage *request, *response;
    krb5_data response_data;

    ret = kcm_storage_request(context, KCM_OP_GET_PRINCIPAL, &request);
    if (ret)
	return ret;

    ret = krb5_store_stringz(request, k->name);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = kcm_call(context, k, request, &response, &response_data);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_ret_principal(response, principal);
    if (ret)
	ret = KRB5_CC_IO;

    krb5_storage_free(request);
    krb5_storage_free(response);
    krb5_data_free(&response_data);

    return ret;
}

/*
 * Request:
 *      NameZ
 *
 * Response:
 *      Cursor
 *
 */
static krb5_error_code
kcm_get_first (krb5_context context,
	       krb5_ccache id,
	       krb5_cc_cursor *cursor)
{
    krb5_error_code ret;
    krb5_kcmcache *k = KCMCACHE(id);
    krb5_storage *request, *response;
    krb5_data response_data;
    int32_t tmp;

    ret = kcm_storage_request(context, KCM_OP_GET_FIRST, &request);
    if (ret)
	return ret;

    ret = krb5_store_stringz(request, k->name);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = kcm_call(context, k, request, &response, &response_data);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_ret_int32(response, &tmp);
    if (ret || tmp < 0)
	ret = KRB5_CC_IO;

    krb5_storage_free(request);
    krb5_storage_free(response);
    krb5_data_free(&response_data);

    if (ret)
	return ret;

    *cursor = malloc(sizeof(tmp));
    if (*cursor == NULL)
	return KRB5_CC_NOMEM;

    KCMCURSOR(*cursor) = tmp;

    return 0;
}

/*
 * Request:
 *      NameZ
 *      Cursor
 *
 * Response:
 *      Creds
 */
static krb5_error_code
kcm_get_next (krb5_context context,
		krb5_ccache id,
		krb5_cc_cursor *cursor,
		krb5_creds *creds)
{
    krb5_error_code ret;
    krb5_kcmcache *k = KCMCACHE(id);
    krb5_storage *request, *response;
    krb5_data response_data;

    ret = kcm_storage_request(context, KCM_OP_GET_NEXT, &request);
    if (ret)
	return ret;

    ret = krb5_store_stringz(request, k->name);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_store_int32(request, KCMCURSOR(*cursor));
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = kcm_call(context, k, request, &response, &response_data);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_ret_creds(response, creds);
    if (ret)
	ret = KRB5_CC_IO;

    krb5_storage_free(request);
    krb5_storage_free(response);
    krb5_data_free(&response_data);

    return ret;
}

/*
 * Request:
 *      NameZ
 *      Cursor
 *
 * Response:
 *
 */
static krb5_error_code
kcm_end_get (krb5_context context,
	     krb5_ccache id,
	     krb5_cc_cursor *cursor)
{
    krb5_error_code ret;
    krb5_kcmcache *k = KCMCACHE(id);
    krb5_storage *request;

    ret = kcm_storage_request(context, KCM_OP_END_GET, &request);
    if (ret)
	return ret;

    ret = krb5_store_stringz(request, k->name);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_store_int32(request, KCMCURSOR(*cursor));
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = kcm_call(context, k, request, NULL, NULL);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }
  
    krb5_storage_free(request);

    KCMCURSOR(*cursor) = 0;
    free(*cursor);
    *cursor = NULL;

    return ret;
}

/*
 * Request:
 *      NameZ
 *      WhichFields
 *      MatchCreds
 *
 * Response:
 *
 */
static krb5_error_code
kcm_remove_cred(krb5_context context,
		krb5_ccache id,
		krb5_flags which,
		krb5_creds *cred)
{
    krb5_error_code ret;
    krb5_kcmcache *k = KCMCACHE(id);
    krb5_storage *request;

    ret = kcm_storage_request(context, KCM_OP_REMOVE_CRED, &request);
    if (ret)
	return ret;

    ret = krb5_store_stringz(request, k->name);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_store_int32(request, which);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_store_creds_tag(request, cred);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = kcm_call(context, k, request, NULL, NULL);

    krb5_storage_free(request);
    return ret;
}

static krb5_error_code
kcm_set_flags(krb5_context context,
	      krb5_ccache id,
	      krb5_flags flags)
{
    krb5_error_code ret;
    krb5_kcmcache *k = KCMCACHE(id);
    krb5_storage *request;

    ret = kcm_storage_request(context, KCM_OP_SET_FLAGS, &request);
    if (ret)
	return ret;

    ret = krb5_store_stringz(request, k->name);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_store_int32(request, flags);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = kcm_call(context, k, request, NULL, NULL);

    krb5_storage_free(request);
    return ret;
}

static krb5_error_code
kcm_get_version(krb5_context context,
		krb5_ccache id)
{
    return 0;
}

static krb5_error_code
kcm_move(krb5_context context, krb5_ccache from, krb5_ccache to)
{
    krb5_set_error_string(context, "kcm_move not implemented");
    return EINVAL;
}

static krb5_error_code
kcm_default_name(krb5_context context, char **str)
{
    return _krb5_expand_default_cc_name(context, 
					KRB5_DEFAULT_CCNAME_KCM,
					str);
}

/**
 * Variable containing the KCM based credential cache implemention.
 *
 * @ingroup krb5_ccache
 */

const krb5_cc_ops krb5_kcm_ops = {
    "KCM",
    kcm_get_name,
    kcm_resolve,
    kcm_gen_new,
    kcm_initialize,
    kcm_destroy,
    kcm_close,
    kcm_store_cred,
    kcm_retrieve,
    kcm_get_principal,
    kcm_get_first,
    kcm_get_next,
    kcm_end_get,
    kcm_remove_cred,
    kcm_set_flags,
    kcm_get_version,
    NULL,
    NULL,
    NULL,
    kcm_move,
    kcm_default_name
};

krb5_boolean
_krb5_kcm_is_running(krb5_context context)
{
    krb5_error_code ret;
    krb5_ccache_data ccdata;
    krb5_ccache id = &ccdata;
    krb5_boolean running;

    ret = kcm_alloc(context, NULL, &id);
    if (ret)
	return 0;

    running = (_krb5_kcm_noop(context, id) == 0);

    kcm_free(context, &id);

    return running;
}

/*
 * Request:
 *
 * Response:
 *
 */
krb5_error_code
_krb5_kcm_noop(krb5_context context,
	       krb5_ccache id)
{
    krb5_error_code ret;
    krb5_kcmcache *k = KCMCACHE(id);
    krb5_storage *request;

    ret = kcm_storage_request(context, KCM_OP_NOOP, &request);
    if (ret)
	return ret;

    ret = kcm_call(context, k, request, NULL, NULL);

    krb5_storage_free(request);
    return ret;
}


/*
 * Request:
 *      NameZ
 *      Mode
 *
 * Response:
 *
 */
krb5_error_code
_krb5_kcm_chmod(krb5_context context,
		krb5_ccache id,
		uint16_t mode)
{
    krb5_error_code ret;
    krb5_kcmcache *k = KCMCACHE(id);
    krb5_storage *request;

    ret = kcm_storage_request(context, KCM_OP_CHMOD, &request);
    if (ret)
	return ret;

    ret = krb5_store_stringz(request, k->name);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_store_int16(request, mode);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = kcm_call(context, k, request, NULL, NULL);

    krb5_storage_free(request);
    return ret;
}


/*
 * Request:
 *      NameZ
 *      UID
 *      GID
 *
 * Response:
 *
 */
krb5_error_code
_krb5_kcm_chown(krb5_context context,
		krb5_ccache id,
		uint32_t uid,
		uint32_t gid)
{
    krb5_error_code ret;
    krb5_kcmcache *k = KCMCACHE(id);
    krb5_storage *request;

    ret = kcm_storage_request(context, KCM_OP_CHOWN, &request);
    if (ret)
	return ret;

    ret = krb5_store_stringz(request, k->name);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_store_int32(request, uid);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_store_int32(request, gid);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = kcm_call(context, k, request, NULL, NULL);

    krb5_storage_free(request);
    return ret;
}


/*
 * Request:
 *      NameZ
 *      ServerPrincipalPresent
 *      ServerPrincipal OPTIONAL
 *      Key
 *
 * Repsonse:
 *
 */
krb5_error_code
_krb5_kcm_get_initial_ticket(krb5_context context,
			     krb5_ccache id,
			     krb5_principal server,
			     krb5_keyblock *key)
{
    krb5_error_code ret;
    krb5_kcmcache *k = KCMCACHE(id);
    krb5_storage *request;

    ret = kcm_storage_request(context, KCM_OP_GET_INITIAL_TICKET, &request);
    if (ret)
	return ret;

    ret = krb5_store_stringz(request, k->name);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_store_int8(request, (server == NULL) ? 0 : 1);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    if (server != NULL) {
	ret = krb5_store_principal(request, server);
	if (ret) {
	    krb5_storage_free(request);
	    return ret;
	}
    }

    ret = krb5_store_keyblock(request, *key);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = kcm_call(context, k, request, NULL, NULL);

    krb5_storage_free(request);
    return ret;
}


/*
 * Request:
 *      NameZ
 *      KDCFlags
 *      EncryptionType
 *      ServerPrincipal
 *
 * Repsonse:
 *
 */
krb5_error_code
_krb5_kcm_get_ticket(krb5_context context,
		     krb5_ccache id,
		     krb5_kdc_flags flags,
		     krb5_enctype enctype,
		     krb5_principal server)
{
    krb5_error_code ret;
    krb5_kcmcache *k = KCMCACHE(id);
    krb5_storage *request;

    ret = kcm_storage_request(context, KCM_OP_GET_TICKET, &request);
    if (ret)
	return ret;

    ret = krb5_store_stringz(request, k->name);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_store_int32(request, flags.i);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_store_int32(request, enctype);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = krb5_store_principal(request, server);
    if (ret) {
	krb5_storage_free(request);
	return ret;
    }

    ret = kcm_call(context, k, request, NULL, NULL);

    krb5_storage_free(request);
    return ret;
}


#endif /* HAVE_KCM */
