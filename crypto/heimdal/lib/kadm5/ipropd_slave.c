/*
 * Copyright (c) 1997, 1998 Kungliga Tekniska Högskolan
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

RCSID("$Id: ipropd_slave.c,v 1.10 1999/12/02 17:05:06 joda Exp $");

static int
connect_to_master (krb5_context context, const char *master)
{
    int fd;
    struct sockaddr_in addr;
    struct hostent *he;

    fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
	krb5_err (context, 1, errno, "socket AF_INET");
    memset (&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(4711);
    he = roken_gethostbyname (master);
    if (he == NULL)
	krb5_errx (context, 1, "gethostbyname: %s", hstrerror(h_errno));
    memcpy (&addr.sin_addr, he->h_addr, sizeof(addr.sin_addr));
    if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	krb5_err (context, 1, errno, "connect");
    return fd;
}

static void
get_creds(krb5_context context, krb5_ccache *cache, const char *host)
{
    krb5_keytab keytab;
    krb5_principal client;
    krb5_error_code ret;
    krb5_get_init_creds_opt init_opts;
#if 0
    krb5_preauthtype preauth = KRB5_PADATA_ENC_TIMESTAMP;
#endif
    krb5_creds creds;
    char my_hostname[128];
    char *server;
    
    ret = krb5_kt_default(context, &keytab);
    if(ret) krb5_err(context, 1, ret, "krb5_kt_default");

    gethostname (my_hostname, sizeof(my_hostname));
    ret = krb5_sname_to_principal (context, my_hostname, IPROP_NAME,
				   KRB5_NT_SRV_HST, &client);
    if (ret) krb5_err(context, 1, ret, "krb5_sname_to_principal");

    krb5_get_init_creds_opt_init(&init_opts);
#if 0
    krb5_get_init_creds_opt_set_preauth_list(&init_opts, &preauth, 1);
#endif

    asprintf (&server, "%s/%s", IPROP_NAME, host);
    if (server == NULL)
	krb5_errx (context, 1, "malloc: no memory");

    ret = krb5_get_init_creds_keytab(context, &creds, client, keytab,
				     0, server, &init_opts);
    free (server);
    if(ret) krb5_err(context, 1, ret, "krb5_get_init_creds");
    
    ret = krb5_kt_close(context, keytab);
    if(ret) krb5_err(context, 1, ret, "krb5_kt_close");
    
    ret = krb5_cc_gen_new(context, &krb5_mcc_ops, cache);
    if(ret) krb5_err(context, 1, ret, "krb5_cc_gen_new");

    ret = krb5_cc_initialize(context, *cache, client);
    if(ret) krb5_err(context, 1, ret, "krb5_cc_initialize");

    ret = krb5_cc_store_cred(context, *cache, &creds);
    if(ret) krb5_err(context, 1, ret, "krb5_cc_store_cred");
}

static void
ihave (krb5_context context, krb5_auth_context auth_context,
       int fd, u_int32_t version)
{
    int ret;
    u_char buf[8];
    krb5_storage *sp;
    krb5_data data, priv_data;

    sp = krb5_storage_from_mem (buf, 8);
    krb5_store_int32 (sp, I_HAVE);
    krb5_store_int32 (sp, version);
    krb5_storage_free (sp);
    data.length = 8;
    data.data   = buf;
    
    ret = krb5_mk_priv (context, auth_context, &data, &priv_data, NULL);
    if (ret)
	krb5_err (context, 1, ret, "krb_mk_priv");

    ret = krb5_write_message (context, &fd, &priv_data);
    if (ret)
	krb5_err (context, 1, ret, "krb5_write_message");

    krb5_data_free (&priv_data);
}

static void
receive (krb5_context context,
	 krb5_storage *sp,
	 kadm5_server_context *server_context)
{
    int ret;
    off_t left, right;
    void *buf;
    int32_t vers;

    ret = server_context->db->open(context,
				   server_context->db,
				   O_RDWR | O_CREAT, 0);
    if (ret)
	krb5_err (context, 1, ret, "db->open");

    do {
	int32_t len, timestamp, tmp;
	enum kadm_ops op;

	if(krb5_ret_int32 (sp, &vers) != 0)
	    return;
	krb5_ret_int32 (sp, &timestamp);
	krb5_ret_int32 (sp, &tmp);
	op = tmp;
	krb5_ret_int32 (sp, &len);
	if (vers <= server_context->log_context.version)
	    sp->seek(sp, len, SEEK_CUR);
    } while(vers <= server_context->log_context.version);

    left  = sp->seek (sp, -16, SEEK_CUR);
    right = sp->seek (sp, 0, SEEK_END);
    buf = malloc (right - left);
    if (buf == NULL) {
	krb5_warnx (context, "malloc: no memory");
	return;
    }
    sp->seek (sp, left, SEEK_SET);
    sp->fetch (sp, buf, right - left);
    write (server_context->log_context.log_fd, buf, right-left);
    fsync (server_context->log_context.log_fd);
    free (buf);

    sp->seek (sp, left, SEEK_SET);

    for(;;) {
	int32_t len, timestamp, tmp;
	enum kadm_ops op;

	if(krb5_ret_int32 (sp, &vers) != 0)
	    break;
	krb5_ret_int32 (sp, &timestamp);
	krb5_ret_int32 (sp, &tmp);
	op = tmp;
	krb5_ret_int32 (sp, &len);

	ret = kadm5_log_replay (server_context,
				op, vers, len, sp);
	if (ret)
	    krb5_warn (context, ret, "kadm5_log_replay");
	else
	    server_context->log_context.version = vers;
	sp->seek (sp, 8, SEEK_CUR);
    }

    ret = server_context->db->close (context, server_context->db);
    if (ret)
	krb5_err (context, 1, ret, "db->close");
}

char *realm;
int version_flag;
int help_flag;
struct getargs args[] = {
    { "realm", 'r', arg_string, &realm },
    { "version", 0, arg_flag, &version_flag },
    { "help", 0, arg_flag, &help_flag }
};
int num_args = sizeof(args) / sizeof(args[0]);

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_auth_context auth_context;
    void *kadm_handle;
    kadm5_server_context *server_context;
    kadm5_config_params conf;
    int master_fd;
    krb5_ccache ccache;
    krb5_principal server;

    int optind;
    
    optind = krb5_program_setup(&context, argc, argv, args, num_args, NULL);
    
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

    get_creds(context, &ccache, argv[1]);

    master_fd = connect_to_master (context, argv[1]);

    ret = krb5_sname_to_principal (context, argv[1], IPROP_NAME,
				   KRB5_NT_SRV_HST, &server);
    if (ret)
	krb5_err (context, 1, ret, "krb5_sname_to_principal");

    auth_context = NULL;
    ret = krb5_sendauth (context, &auth_context, &master_fd,
			 IPROP_VERSION, NULL, server,
			 AP_OPTS_MUTUAL_REQUIRED, NULL, NULL,
			 ccache, NULL, NULL, NULL);
    if (ret)
	krb5_err (context, 1, ret, "krb5_sendauth");

    ihave (context, auth_context, master_fd,
	   server_context->log_context.version);

    for (;;) {
	int ret;
	krb5_data data, out;
	krb5_storage *sp;
	int32_t tmp;

	ret = krb5_read_message (context, &master_fd, &data);
	if (ret)
	    krb5_err (context, 1, ret, "krb5_read_message");

	ret = krb5_rd_priv (context, auth_context,  &data, &out, NULL);
	krb5_data_free (&data);
	if (ret)
	    krb5_err (context, 1, ret, "krb5_rd_priv");

	sp = krb5_storage_from_mem (out.data, out.length);
	krb5_ret_int32 (sp, &tmp);
	switch (tmp) {
	case FOR_YOU :
	    receive (context, sp, server_context);
	    ihave (context, auth_context, master_fd,
		   server_context->log_context.version);
	    break;
	case I_HAVE :
	default :
	    krb5_warnx (context, "Ignoring command %d", tmp);
	    break;
	}
	krb5_storage_free (sp);
	krb5_data_free (&out);
    }

    return 0;
}
