/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska Högskolan
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

RCSID("$Id: ipropd_slave.c,v 1.21 2000/08/06 02:06:19 assar Exp $");

static krb5_log_facility *log_facility;

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
    addr.sin_port   = krb5_getportbyname (context,
					  IPROP_SERVICE, "tcp", IPROP_PORT);
    he = roken_gethostbyname (master);
    if (he == NULL)
	krb5_errx (context, 1, "gethostbyname: %s", hstrerror(h_errno));
    memcpy (&addr.sin_addr, he->h_addr, sizeof(addr.sin_addr));
    if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	krb5_err (context, 1, errno, "connect");
    return fd;
}

static void
get_creds(krb5_context context, const char *keytab_str,
	  krb5_ccache *cache, const char *host)
{
    krb5_keytab keytab;
    krb5_principal client;
    krb5_error_code ret;
    krb5_get_init_creds_opt init_opts;
    krb5_creds creds;
    char *server;
    char keytab_buf[256];
    
    ret = krb5_kt_register(context, &hdb_kt_ops);
    if(ret)
	krb5_err(context, 1, ret, "krb5_kt_register");

    if (keytab_str == NULL) {
	ret = krb5_kt_default_name (context, keytab_buf, sizeof(keytab_buf));
	if (ret)
	    krb5_err (context, 1, ret, "krb5_kt_default_name");
	keytab_str = keytab_buf;
    }

    ret = krb5_kt_resolve(context, keytab_str, &keytab);
    if(ret)
	krb5_err(context, 1, ret, "%s", keytab_str);
    
    ret = krb5_sname_to_principal (context, NULL, IPROP_NAME,
				   KRB5_NT_SRV_HST, &client);
    if (ret) krb5_err(context, 1, ret, "krb5_sname_to_principal");

    krb5_get_init_creds_opt_init(&init_opts);

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
receive_loop (krb5_context context,
	      krb5_storage *sp,
	      kadm5_server_context *server_context)
{
    int ret;
    off_t left, right;
    void *buf;
    int32_t vers;

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
    if (buf == NULL && (right - left) != 0) {
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
}

static void
receive (krb5_context context,
	 krb5_storage *sp,
	 kadm5_server_context *server_context)
{
    int ret;

    ret = server_context->db->open(context,
				   server_context->db,
				   O_RDWR | O_CREAT, 0600);
    if (ret)
	krb5_err (context, 1, ret, "db->open");

    receive_loop (context, sp, server_context);

    ret = server_context->db->close (context, server_context->db);
    if (ret)
	krb5_err (context, 1, ret, "db->close");
}

static void
receive_everything (krb5_context context, int fd,
		    kadm5_server_context *server_context,
		    krb5_auth_context auth_context)
{
    int ret;
    krb5_data data;
    int32_t vno;
    int32_t opcode;

    ret = server_context->db->open(context,
				   server_context->db,
				   O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (ret)
	krb5_err (context, 1, ret, "db->open");

    do {
	krb5_storage *sp;

	ret = krb5_read_priv_message(context, auth_context, &fd, &data);

	if (ret)
	    krb5_err (context, 1, ret, "krb5_read_priv_message");

	sp = krb5_storage_from_data (&data);
	krb5_ret_int32 (sp, &opcode);
	if (opcode == ONE_PRINC) {
	    krb5_data fake_data;
	    hdb_entry entry;

	    fake_data.data   = (char *)data.data + 4;
	    fake_data.length = data.length - 4;

	    ret = hdb_value2entry (context, &fake_data, &entry);
	    if (ret)
		krb5_err (context, 1, ret, "hdb_value2entry");
	    ret = server_context->db->store(server_context->context,
					    server_context->db,
					    0, &entry);
	    if (ret)
		krb5_err (context, 1, ret, "hdb_store");

	    hdb_free_entry (context, &entry);
	    krb5_data_free (&data);
	}
    } while (opcode == ONE_PRINC);

    if (opcode != NOW_YOU_HAVE)
	krb5_errx (context, 1, "receive_everything: strange %d", opcode);

    _krb5_get_int ((char *)data.data + 4, &vno, 4);

    ret = kadm5_log_reinit (server_context);
    if (ret)
	krb5_err(context, 1, ret, "kadm5_log_reinit");

    ret = kadm5_log_set_version (server_context, vno - 1);
    if (ret)
	krb5_err (context, 1, ret, "kadm5_log_set_version");

    ret = kadm5_log_nop (server_context);
    if (ret)
	krb5_err (context, 1, ret, "kadm5_log_nop");

    krb5_data_free (&data);

    ret = server_context->db->close (context, server_context->db);
    if (ret)
	krb5_err (context, 1, ret, "db->close");
}

static char *realm;
static int version_flag;
static int help_flag;
static char *keytab_str;

static struct getargs args[] = {
    { "realm", 'r', arg_string, &realm },
    { "keytab", 'k', arg_string, &keytab_str,
      "keytab to get authentication from", "kspec" },
    { "version", 0, arg_flag, &version_flag },
    { "help", 0, arg_flag, &help_flag }
};

static int num_args = sizeof(args) / sizeof(args[0]);

static void
usage (int code, struct getargs *args, int num_args)
{
    arg_printusage (args, num_args, NULL, "master");
    exit (code);
}

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
    const char *master;
    
    optind = krb5_program_setup(&context, argc, argv, args, num_args, usage);
    
    if(help_flag)
	usage (0, args, num_args);
    if(version_flag) {
	print_version(NULL);
	exit(0);
    }

    argc -= optind;
    argv += optind;

    if (argc != 1)
	usage (1, args, num_args);

    master = argv[0];

    krb5_openlog (context, "ipropd-master", &log_facility);
    krb5_set_warn_dest(context, log_facility);

    ret = krb5_kt_register(context, &hdb_kt_ops);
    if(ret)
	krb5_err(context, 1, ret, "krb5_kt_register");

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

    get_creds(context, keytab_str, &ccache, master);

    master_fd = connect_to_master (context, master);

    ret = krb5_sname_to_principal (context, master, IPROP_NAME,
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
	krb5_data out;
	krb5_storage *sp;
	int32_t tmp;

	ret = krb5_read_priv_message(context, auth_context, &master_fd, &out);

	if (ret)
	    krb5_err (context, 1, ret, "krb5_read_priv_message");

	sp = krb5_storage_from_mem (out.data, out.length);
	krb5_ret_int32 (sp, &tmp);
	switch (tmp) {
	case FOR_YOU :
	    receive (context, sp, server_context);
	    ihave (context, auth_context, master_fd,
		   server_context->log_context.version);
	    break;
	case TELL_YOU_EVERYTHING :
	    receive_everything (context, master_fd, server_context,
				auth_context);
	    break;
	case NOW_YOU_HAVE :
	case I_HAVE :
	case ONE_PRINC :
	default :
	    krb5_warnx (context, "Ignoring command %d", tmp);
	    break;
	}
	krb5_storage_free (sp);
	krb5_data_free (&out);
    }
    
    return 0;
    }
