/*
 * Copyright (c) 1997 - 2003 Kungliga Tekniska Högskolan
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

#include "iprop.h"
#include "parse_time.h"

RCSID("$Id: dump_log.c,v 1.13 2003/04/16 17:56:02 lha Exp $");

static char *op_names[] = {
    "get",
    "delete",
    "create",
    "rename",
    "chpass",
    "modify",
    "randkey",
    "get_privs",
    "get_princs",
    "chpass_with_key",
    "nop"
};

static void
print_entry(kadm5_server_context *server_context,
	    u_int32_t ver,
	    time_t timestamp,
	    enum kadm_ops op,
	    u_int32_t len,
	    krb5_storage *sp)
{
    char t[256];
    int32_t mask;
    hdb_entry ent;
    krb5_principal source;
    char *name1, *name2;
    krb5_data data;
    krb5_context context = server_context->context;

    off_t end = krb5_storage_seek(sp, 0, SEEK_CUR) + len;
    
    krb5_error_code ret;

    strftime(t, sizeof(t), "%Y-%m-%d %H:%M:%S", localtime(&timestamp));

    if(op < kadm_get || op > kadm_nop) {
	printf("unknown op: %d\n", op);
	krb5_storage_seek(sp, end, SEEK_SET);
	return;
    }

    printf ("%s: ver = %u, timestamp = %s, len = %u\n",
	    op_names[op], ver, t, len);
    switch(op) {
    case kadm_delete:
	krb5_ret_principal(sp, &source);
	krb5_unparse_name(context, source, &name1);
	printf("    %s\n", name1);
	free(name1);
	krb5_free_principal(context, source);
	break;
    case kadm_rename:
	ret = krb5_data_alloc(&data, len);
	if (ret)
	    krb5_err (context, 1, ret, "kadm_rename: data alloc: %d", len);
	krb5_ret_principal(sp, &source);
	krb5_storage_read(sp, data.data, data.length);
	hdb_value2entry(context, &data, &ent);
	krb5_unparse_name(context, source, &name1);
	krb5_unparse_name(context, ent.principal, &name2);
	printf("    %s -> %s\n", name1, name2);
	free(name1);
	free(name2);
	krb5_free_principal(context, source);
	hdb_free_entry(context, &ent);
	break;
    case kadm_create:
	ret = krb5_data_alloc(&data, len);
	if (ret)
	    krb5_err (context, 1, ret, "kadm_create: data alloc: %d", len);
	krb5_storage_read(sp, data.data, data.length);
	ret = hdb_value2entry(context, &data, &ent);
	if(ret)
	    abort();
	mask = ~0;
	goto foo;
    case kadm_modify:
	ret = krb5_data_alloc(&data, len);
	if (ret)
	    krb5_err (context, 1, ret, "kadm_modify: data alloc: %d", len);
	krb5_ret_int32(sp, &mask);
	krb5_storage_read(sp, data.data, data.length);
	ret = hdb_value2entry(context, &data, &ent);
	if(ret)
	    abort();
    foo:
	if(ent.principal /* mask & KADM5_PRINCIPAL */) {
	    krb5_unparse_name(context, ent.principal, &name1);
	    printf("    principal = %s\n", name1);
	    free(name1);
	}
	if(mask & KADM5_PRINC_EXPIRE_TIME) {
	    if(ent.valid_end == NULL) {
		strcpy(t, "never");
	    } else {
		strftime(t, sizeof(t), "%Y-%m-%d %H:%M:%S", 
			 localtime(ent.valid_end));
	    }
	    printf("    expires = %s\n", t);
	}
	if(mask & KADM5_PW_EXPIRATION) {
	    if(ent.pw_end == NULL) {
		strcpy(t, "never");
	    } else {
		strftime(t, sizeof(t), "%Y-%m-%d %H:%M:%S", 
			 localtime(ent.pw_end));
	    }
	    printf("    password exp = %s\n", t);
	}
	if(mask & KADM5_LAST_PWD_CHANGE) {
	}
	if(mask & KADM5_ATTRIBUTES) {
	    unparse_flags(HDBFlags2int(ent.flags), 
			  HDBFlags_units, t, sizeof(t));
	    printf("    attributes = %s\n", t);
	}
	if(mask & KADM5_MAX_LIFE) {
	    if(ent.max_life == NULL)
		strcpy(t, "for ever");
	    else
		unparse_time(*ent.max_life, t, sizeof(t));
	    printf("    max life = %s\n", t);
	}
	if(mask & KADM5_MAX_RLIFE) {
	    if(ent.max_renew == NULL)
		strcpy(t, "for ever");
	    else
		unparse_time(*ent.max_renew, t, sizeof(t));
	    printf("    max rlife = %s\n", t);
	}
	if(mask & KADM5_MOD_TIME) {
	    printf("    mod time\n");
	}
	if(mask & KADM5_MOD_NAME) {
	    printf("    mod name\n");
	}
	if(mask & KADM5_KVNO) {
	    printf("    kvno = %d\n", ent.kvno);
	}
	if(mask & KADM5_MKVNO) {
	    printf("    mkvno\n");
	}
	if(mask & KADM5_AUX_ATTRIBUTES) {
	    printf("    aux attributes\n");
	}
	if(mask & KADM5_POLICY) {
	    printf("    policy\n");
	}
	if(mask & KADM5_POLICY_CLR) {
	    printf("    mod time\n");
	}
	if(mask & KADM5_LAST_SUCCESS) {
	    printf("    last success\n");
	}
	if(mask & KADM5_LAST_FAILED) {
	    printf("    last failed\n");
	}
	if(mask & KADM5_FAIL_AUTH_COUNT) {
	    printf("    fail auth count\n");
	}
	if(mask & KADM5_KEY_DATA) {
	    printf("    key data\n");
	}
	if(mask & KADM5_TL_DATA) {
	    printf("    tl data\n");
	}
	hdb_free_entry(context, &ent);
	break;
    case kadm_nop :
	break;
    default:
	abort();
    }
    krb5_storage_seek(sp, end, SEEK_SET);
}

static char *realm;
static int version_flag;
static int help_flag;

static struct getargs args[] = {
    { "realm", 'r', arg_string, &realm },
    { "version", 0, arg_flag, &version_flag },
    { "help", 0, arg_flag, &help_flag }
};
int num_args = sizeof(args) / sizeof(args[0]);

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;
    void *kadm_handle;
    kadm5_server_context *server_context;
    kadm5_config_params conf;

    krb5_program_setup(&context, argc, argv, args, num_args, NULL);
    
    if(help_flag)
	krb5_std_usage(0, args, num_args);
    if(version_flag) {
	print_version(NULL);
	exit(0);
    }

    memset(&conf, 0, sizeof(conf));
    if(realm) {
	conf.mask |= KADM5_CONFIG_REALM;
	conf.realm = realm;
    }
    ret = kadm5_init_with_password_ctx (context,
					KADM5_ADMIN_SERVICE,
					NULL,
					KADM5_ADMIN_SERVICE,
					&conf, 0, 0, 
					&kadm_handle);
    if (ret)
	krb5_err (context, 1, ret, "kadm5_init_with_password_ctx");

    server_context = (kadm5_server_context *)kadm_handle;

    ret = kadm5_log_init (server_context);
    if (ret)
	krb5_err (context, 1, ret, "kadm5_log_init");

    ret = kadm5_log_foreach (server_context, print_entry);
    if(ret)
	krb5_warn(context, ret, "kadm5_log_foreach");

    ret = kadm5_log_end (server_context);
    if (ret)
	krb5_warn(context, ret, "kadm5_log_end");
    return 0;
}
