/*
 * Copyright (c) 1997 - 2007 Kungliga Tekniska Högskolan
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

RCSID("$Id: ipropd_slave.c 22211 2007-12-07 19:27:27Z lha $");

static krb5_log_facility *log_facility;
static char *server_time_lost = "5 min";
static int time_before_lost;
const char *slave_str = NULL;

static int
connect_to_master (krb5_context context, const char *master,
		   const char *port_str)
{
    int fd;
    struct sockaddr_in addr;
    struct hostent *he;

    fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
	krb5_err (context, 1, errno, "socket AF_INET");
    memset (&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    if (port_str) {
	addr.sin_port = krb5_getportbyname (context,
					    port_str, "tcp", 
					    0);
	if (addr.sin_port == 0) {
	    char *ptr;
	    long port;
	    
	    port = strtol (port_str, &ptr, 10);
	    if (port == 0 && ptr == port_str)
		krb5_errx (context, 1, "bad port `%s'", port_str);
	    addr.sin_port = htons(port);
	}
    } else {
	addr.sin_port = krb5_getportbyname (context, IPROP_SERVICE, 
					    "tcp", IPROP_PORT);
    }
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
	  krb5_ccache *cache, const char *serverhost)
{
    krb5_keytab keytab;
    krb5_principal client;
    krb5_error_code ret;
    krb5_get_init_creds_opt *init_opts;
    krb5_creds creds;
    char *server;
    char keytab_buf[256];
    
    if (keytab_str == NULL) {
	ret = krb5_kt_default_name (context, keytab_buf, sizeof(keytab_buf));
	if (ret)
	    krb5_err (context, 1, ret, "krb5_kt_default_name");
	keytab_str = keytab_buf;
    }

    ret = krb5_kt_resolve(context, keytab_str, &keytab);
    if(ret)
	krb5_err(context, 1, ret, "%s", keytab_str);
    

    ret = krb5_sname_to_principal (context, slave_str, IPROP_NAME,
				   KRB5_NT_SRV_HST, &client);
    if (ret) krb5_err(context, 1, ret, "krb5_sname_to_principal");

    ret = krb5_get_init_creds_opt_alloc(context, &init_opts);
    if (ret) krb5_err(context, 1, ret, "krb5_get_init_creds_opt_alloc");

    asprintf (&server, "%s/%s", IPROP_NAME, serverhost);
    if (server == NULL)
	krb5_errx (context, 1, "malloc: no memory");

    ret = krb5_get_init_creds_keytab(context, &creds, client, keytab,
				     0, server, init_opts);
    free (server);
    krb5_get_init_creds_opt_free(context, init_opts);
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
       int fd, uint32_t version)
{
    int ret;
    u_char buf[8];
    krb5_storage *sp;
    krb5_data data;

    sp = krb5_storage_from_mem (buf, 8);
    krb5_store_int32 (sp, I_HAVE);
    krb5_store_int32 (sp, version);
    krb5_storage_free (sp);
    data.length = 8;
    data.data   = buf;
    
    ret = krb5_write_priv_message(context, auth_context, &fd, &data);
    if (ret)
	krb5_err (context, 1, ret, "krb5_write_priv_message");
}

static void
receive_loop (krb5_context context,
	      krb5_storage *sp,
	      kadm5_server_context *server_context)
{
    int ret;
    off_t left, right;
    void *buf;
    int32_t vers, vers2;
    ssize_t sret;

    /*
     * Seek to the current version of the local database.
     */
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
	    krb5_storage_seek(sp, len + 8, SEEK_CUR);
    } while(vers <= server_context->log_context.version);

    /*
     * Read up rest of the entires into the memory...
     */
    left  = krb5_storage_seek (sp, -16, SEEK_CUR);
    right = krb5_storage_seek (sp, 0, SEEK_END);
    buf = malloc (right - left);
    if (buf == NULL && (right - left) != 0)
	krb5_errx (context, 1, "malloc: no memory");

    /*
     * ...and then write them out to the on-disk log.
     */
    krb5_storage_seek (sp, left, SEEK_SET);
    krb5_storage_read (sp, buf, right - left);
    sret = write (server_context->log_context.log_fd, buf, right-left);
    if (sret != right - left)
	krb5_err(context, 1, errno, "Failed to write log to disk");
    ret = fsync (server_context->log_context.log_fd);
    if (ret)
	krb5_err(context, 1, errno, "Failed to sync log to disk");
    free (buf);

    /*
     * Go back to the startpoint and start to commit the entires to
     * the database.
     */
    krb5_storage_seek (sp, left, SEEK_SET);

    for(;;) {
	int32_t len, len2, timestamp, tmp;
	off_t cur, cur2;
	enum kadm_ops op;

	if(krb5_ret_int32 (sp, &vers) != 0)
	    break;
	ret = krb5_ret_int32 (sp, &timestamp);
	if (ret) krb5_errx(context, 1, "entry %ld: too short", (long)vers);
	ret = krb5_ret_int32 (sp, &tmp);
	if (ret) krb5_errx(context, 1, "entry %ld: too short", (long)vers);
	op = tmp;
	ret = krb5_ret_int32 (sp, &len);
	if (ret) krb5_errx(context, 1, "entry %ld: too short", (long)vers);
	if (len < 0)
	    krb5_errx(context, 1, "log is corrupted, "
			"negative length of entry version %ld: %ld",
			(long)vers, (long)len);
	cur = krb5_storage_seek(sp, 0, SEEK_CUR);

	krb5_warnx (context, "replaying entry %d", (int)vers);

	ret = kadm5_log_replay (server_context,
				op, vers, len, sp);
	if (ret) {
	    char *s = krb5_get_error_message(server_context->context, ret);
	    krb5_warnx (context,
		       "kadm5_log_replay: %ld. Lost entry entry, "
		       "Database out of sync ?: %s (%d)",
			(long)vers, s ? s : "unknown error", ret);
	    krb5_xfree(s);
	}

	{
	    /* 
	     * Make sure the krb5_log_replay does the right thing wrt
	     * reading out data from the sp.
	     */
	    cur2 = krb5_storage_seek(sp, 0, SEEK_CUR);
	    if (cur + len != cur2)
		krb5_errx(context, 1, 
			  "kadm5_log_reply version: %ld didn't read the whole entry",
			  (long)vers);
	}

	if (krb5_ret_int32 (sp, &len2) != 0)
	    krb5_errx(context, 1, "entry %ld: postamble too short", (long)vers);
	if(krb5_ret_int32 (sp, &vers2) != 0)
	    krb5_errx(context, 1, "entry %ld: postamble too short", (long)vers);

	if (len != len2)
	    krb5_errx(context, 1, "entry %ld: len != len2", (long)vers);
	if (vers != vers2)
	    krb5_errx(context, 1, "entry %ld: vers != vers2", (long)vers);
    }

    /*
     * Update version
     */

    server_context->log_context.version = vers;
}

static void
receive (krb5_context context,
	 krb5_storage *sp,
	 kadm5_server_context *server_context)
{
    int ret;

    ret = server_context->db->hdb_open(context,
				       server_context->db,
				       O_RDWR | O_CREAT, 0600);
    if (ret)
	krb5_err (context, 1, ret, "db->open");

    receive_loop (context, sp, server_context);

    ret = server_context->db->hdb_close (context, server_context->db);
    if (ret)
	krb5_err (context, 1, ret, "db->close");
}

static void
send_im_here (krb5_context context, int fd,
	      krb5_auth_context auth_context)
{
    krb5_storage *sp;
    krb5_data data;
    int ret;

    ret = krb5_data_alloc (&data, 4);
    if (ret)
	krb5_err (context, 1, ret, "send_im_here");

    sp = krb5_storage_from_data (&data);
    if (sp == NULL)
	krb5_errx (context, 1, "krb5_storage_from_data");
    krb5_store_int32(sp, I_AM_HERE);
    krb5_storage_free(sp);

    ret = krb5_write_priv_message(context, auth_context, &fd, &data);
    krb5_data_free(&data);

    if (ret)
	krb5_err (context, 1, ret, "krb5_write_priv_message");
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
    krb5_storage *sp;

    char *dbname;
    HDB *mydb;
  
    krb5_warnx(context, "receive complete database");

    asprintf(&dbname, "%s-NEW", server_context->db->hdb_name);
    ret = hdb_create(context, &mydb, dbname);
    if(ret)
	krb5_err(context,1, ret, "hdb_create");
    free(dbname);
 
    ret = hdb_set_master_keyfile (context,
				  mydb, server_context->config.stash_file);
    if(ret)
	krb5_err(context,1, ret, "hdb_set_master_keyfile");
 
    /* I really want to use O_EXCL here, but given that I can't easily clean
       up on error, I won't */
    ret = mydb->hdb_open(context, mydb, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (ret)
	krb5_err (context, 1, ret, "db->open");

    sp = NULL;
    do {
	ret = krb5_read_priv_message(context, auth_context, &fd, &data);

	if (ret)
	    krb5_err (context, 1, ret, "krb5_read_priv_message");

	sp = krb5_storage_from_data (&data);
	if (sp == NULL)
	    krb5_errx (context, 1, "krb5_storage_from_data");
	krb5_ret_int32 (sp, &opcode);
	if (opcode == ONE_PRINC) {
	    krb5_data fake_data;
	    hdb_entry_ex entry;

	    krb5_storage_free(sp);

	    fake_data.data   = (char *)data.data + 4;
	    fake_data.length = data.length - 4;

	    memset(&entry, 0, sizeof(entry));

	    ret = hdb_value2entry (context, &fake_data, &entry.entry);
	    if (ret)
		krb5_err (context, 1, ret, "hdb_value2entry");
	    ret = mydb->hdb_store(server_context->context,
				  mydb,
				  0, &entry);
	    if (ret)
		krb5_err (context, 1, ret, "hdb_store");

	    hdb_free_entry (context, &entry);
	    krb5_data_free (&data);
	} else if (opcode == NOW_YOU_HAVE)
	    ;
	else
	    krb5_errx (context, 1, "strange opcode %d", opcode);
    } while (opcode == ONE_PRINC);

    if (opcode != NOW_YOU_HAVE)
	krb5_errx (context, 1, "receive_everything: strange %d", opcode);

    krb5_ret_int32 (sp, &vno);
    krb5_storage_free(sp);

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

    ret = mydb->hdb_rename (context, mydb, server_context->db->hdb_name);
    if (ret)
	krb5_err (context, 1, ret, "db->rename");

    ret = mydb->hdb_close (context, mydb);
    if (ret)
	krb5_err (context, 1, ret, "db->close");

    ret = mydb->hdb_destroy (context, mydb);
    if (ret)
	krb5_err (context, 1, ret, "db->destroy");

    krb5_warnx(context, "receive complete database, version %ld", (long)vno);
}

static char *config_file;
static char *realm;
static int version_flag;
static int help_flag;
static char *keytab_str;
static char *port_str;
static int detach_from_console = 0;

static struct getargs args[] = {
    { "config-file", 'c', arg_string, &config_file },
    { "realm", 'r', arg_string, &realm },
    { "keytab", 'k', arg_string, &keytab_str,
      "keytab to get authentication from", "kspec" },
    { "time-lost", 0, arg_string, &server_time_lost,
      "time before server is considered lost", "time" },
    { "port", 0, arg_string, &port_str,
      "port ipropd-slave will connect to", "port"},
    { "detach", 0, arg_flag, &detach_from_console, 
      "detach from console" },
    { "hostname", 0, arg_string, &slave_str, 
      "hostname of slave (if not same as hostname)", "hostname" },
    { "version", 0, arg_flag, &version_flag },
    { "help", 0, arg_flag, &help_flag }
};

static int num_args = sizeof(args) / sizeof(args[0]);

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
    char **files;
    int optidx;

    const char *master;
    
    optidx = krb5_program_setup(&context, argc, argv, args, num_args, NULL);
    
    if(help_flag)
	krb5_std_usage(0, args, num_args);
    if(version_flag) {
	print_version(NULL);
	exit(0);
    }

    setup_signal();

    if (config_file == NULL) {
	asprintf(&config_file, "%s/kdc.conf", hdb_db_dir(context));
	if (config_file == NULL)
	    errx(1, "out of memory");
    }

    ret = krb5_prepend_config_files_default(config_file, &files);
    if (ret)
	krb5_err(context, 1, ret, "getting configuration files");

    ret = krb5_set_config_files(context, files);
    krb5_free_config_files(files);
    if (ret)
	krb5_err(context, 1, ret, "reading configuration files");

    argc -= optidx;
    argv += optidx;

    if (argc != 1)
	krb5_std_usage(1, args, num_args);

    master = argv[0];

    if (detach_from_console)
	daemon(0, 0);
    pidfile (NULL);
    krb5_openlog (context, "ipropd-slave", &log_facility);
    krb5_set_warn_dest(context, log_facility);

    ret = krb5_kt_register(context, &hdb_kt_ops);
    if(ret)
	krb5_err(context, 1, ret, "krb5_kt_register");

    time_before_lost = parse_time (server_time_lost,  "s");
    if (time_before_lost < 0)
	krb5_errx (context, 1, "couldn't parse time: %s", server_time_lost);

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

    master_fd = connect_to_master (context, master, port_str);

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

    krb5_warnx(context, "ipropd-slave started at version: %ld",
	       (long)server_context->log_context.version);

    ihave (context, auth_context, master_fd,
	   server_context->log_context.version);

    while (exit_flag == 0) {
	krb5_data out;
	krb5_storage *sp;
	int32_t tmp;
	fd_set readset;
	struct timeval to;

	if (master_fd >= FD_SETSIZE)
	    krb5_errx (context, 1, "fd too large");

	FD_ZERO(&readset);
	FD_SET(master_fd, &readset);

	to.tv_sec = time_before_lost;
	to.tv_usec = 0;

	ret = select (master_fd + 1,
		      &readset, NULL, NULL, &to);
	if (ret < 0) {
	    if (errno == EINTR)
		continue;
	    else
		krb5_err (context, 1, errno, "select");
	}
	if (ret == 0)
	    krb5_errx (context, 1, "server didn't send a message "
		       "in %d seconds", time_before_lost);

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
	case ARE_YOU_THERE :
	    send_im_here (context, master_fd, auth_context);
	    break;
	case NOW_YOU_HAVE :
	case I_HAVE :
	case ONE_PRINC :
	case I_AM_HERE :
	default :
	    krb5_warnx (context, "Ignoring command %d", tmp);
	    break;
	}
	krb5_storage_free (sp);
	krb5_data_free (&out);
    }
    
    if(exit_flag == SIGXCPU)
	krb5_warnx(context, "%s CPU time limit exceeded", getprogname());
    else if(exit_flag == SIGINT || exit_flag == SIGTERM)
	krb5_warnx(context, "%s terminated", getprogname());
    else
	krb5_warnx(context, "%s unexpected exit reason: %d", 
		   getprogname(), exit_flag);

    return 0;
}
