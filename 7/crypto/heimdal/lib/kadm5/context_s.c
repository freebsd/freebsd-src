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

#include "kadm5_locl.h"

RCSID("$Id: context_s.c,v 1.17 2002/08/26 13:28:36 assar Exp $");

static void
set_funcs(kadm5_server_context *c)
{
#define SET(C, F) (C)->funcs.F = kadm5_s_ ## F
    SET(c, chpass_principal);
    SET(c, chpass_principal_with_key);
    SET(c, create_principal);
    SET(c, delete_principal);
    SET(c, destroy);
    SET(c, flush);
    SET(c, get_principal);
    SET(c, get_principals);
    SET(c, get_privs);
    SET(c, modify_principal);
    SET(c, randkey_principal);
    SET(c, rename_principal);
}

struct database_spec {
    char *dbpath;
    char *logfile;
    char *mkeyfile;
    char *aclfile;
};

static void
set_field(krb5_context context, krb5_config_binding *binding, 
	  const char *dbname, const char *name, const char *ext, 
	  char **variable)
{
    const char *p;

    if (*variable != NULL)
	free (*variable);

    p = krb5_config_get_string(context, binding, name, NULL);
    if(p)
	*variable = strdup(p);
    else {
	p = strrchr(dbname, '.');
	if(p == NULL)
	    asprintf(variable, "%s.%s", dbname, ext);
	else
	    asprintf(variable, "%.*s.%s", (int)(p - dbname), dbname, ext);
    }
}

static void
set_socket_name(const char *dbname, struct sockaddr_un *un)
{
    const char *p;
    memset(un, 0, sizeof(*un));
    un->sun_family = AF_UNIX;
    p = strrchr(dbname, '.');
    if(p == NULL)
	snprintf(un->sun_path, sizeof(un->sun_path), "%s.signal", 
		 dbname);
    else
	snprintf(un->sun_path, sizeof(un->sun_path), "%.*s.signal", 
		 (int)(p - dbname), dbname);
}

static void
set_config(kadm5_server_context *ctx,
	   krb5_config_binding *binding)
{
    const char *p;
    if(ctx->config.dbname == NULL) {
	p = krb5_config_get_string(ctx->context, binding, "dbname", NULL);
	if(p)
	    ctx->config.dbname = strdup(p);
	else
	    ctx->config.dbname = strdup(HDB_DEFAULT_DB);
    }
    if(ctx->log_context.log_file == NULL)
	set_field(ctx->context, binding, ctx->config.dbname, 
		  "log_file", "log", &ctx->log_context.log_file);
    set_socket_name(ctx->config.dbname, &ctx->log_context.socket_name);
    if(ctx->config.acl_file == NULL)
	set_field(ctx->context, binding, ctx->config.dbname, 
		  "acl_file", "acl", &ctx->config.acl_file);
    if(ctx->config.stash_file == NULL)
	set_field(ctx->context, binding, ctx->config.dbname, 
		  "mkey_file", "mkey", &ctx->config.stash_file);
}

static kadm5_ret_t
find_db_spec(kadm5_server_context *ctx)
{
    const krb5_config_binding *top_binding = NULL;
    krb5_config_binding *db_binding;
    krb5_config_binding *default_binding = NULL;
    krb5_context context = ctx->context;

    while((db_binding = (krb5_config_binding *)
	   krb5_config_get_next(context,
				NULL,
				&top_binding, 
				krb5_config_list, 
				"kdc", 
				"database",
				NULL))) {
	const char *p;
	p = krb5_config_get_string(context, db_binding, "realm", NULL);
	if(p == NULL) {
	    if(default_binding) {
		krb5_warnx(context, "WARNING: more than one realm-less "
			   "database specification");
		krb5_warnx(context, "WARNING: using the first encountered");
	    } else
		default_binding = db_binding;
	    continue;
	}
	if(strcmp(ctx->config.realm, p) != 0)
	    continue;
	
	set_config(ctx, db_binding);
	return 0;
    }
    if(default_binding)
	set_config(ctx, default_binding);
    else {
	ctx->config.dbname        = strdup(HDB_DEFAULT_DB);
	ctx->config.acl_file      = strdup(HDB_DB_DIR "/kadmind.acl");
	ctx->config.stash_file    = strdup(HDB_DB_DIR "/m-key");
	ctx->log_context.log_file = strdup(HDB_DB_DIR "/log");
	memset(&ctx->log_context.socket_name, 0, 
	       sizeof(ctx->log_context.socket_name));
	ctx->log_context.socket_name.sun_family = AF_UNIX;
	strlcpy(ctx->log_context.socket_name.sun_path, 
		KADM5_LOG_SIGNAL, 
		sizeof(ctx->log_context.socket_name.sun_path));
    }
    return 0;
}

kadm5_ret_t
_kadm5_s_init_context(kadm5_server_context **ctx, 
		      kadm5_config_params *params,
		      krb5_context context)
{
    *ctx = malloc(sizeof(**ctx));
    if(*ctx == NULL)
	return ENOMEM;
    memset(*ctx, 0, sizeof(**ctx));
    set_funcs(*ctx);
    (*ctx)->context = context;
    krb5_add_et_list (context, initialize_kadm5_error_table_r);
#define is_set(M) (params && params->mask & KADM5_CONFIG_ ## M)
    if(is_set(REALM))
	(*ctx)->config.realm = strdup(params->realm);
    else
	krb5_get_default_realm(context, &(*ctx)->config.realm);
    if(is_set(DBNAME))
	(*ctx)->config.dbname = strdup(params->dbname);
    if(is_set(ACL_FILE))
	(*ctx)->config.acl_file = strdup(params->acl_file);
    if(is_set(STASH_FILE))
	(*ctx)->config.stash_file = strdup(params->stash_file);
    
    find_db_spec(*ctx);
    
    /* PROFILE can't be specified for now */
    /* KADMIND_PORT is supposed to be used on the server also, 
       but this doesn't make sense */
    /* ADMIN_SERVER is client only */
    /* ADNAME is not used at all (as far as I can tell) */
    /* ADB_LOCKFILE ditto */
    /* DICT_FILE */
    /* ADMIN_KEYTAB */
    /* MKEY_FROM_KEYBOARD is not supported */
    /* MKEY_NAME neither */
    /* ENCTYPE */
    /* MAX_LIFE */
    /* MAX_RLIFE */
    /* EXPIRATION */
    /* FLAGS */
    /* ENCTYPES */

    return 0;
}

HDB *
_kadm5_s_get_db(void *server_handle)
{
    kadm5_server_context *context = server_handle;
    return context->db;
}
