/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
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

RCSID("$Id: ipropd_master.c,v 1.22 2001/02/14 23:00:16 assar Exp $");

static krb5_log_facility *log_facility;

static int
make_signal_socket (krb5_context context)
{
    struct sockaddr_un addr;
    int fd;

    fd = socket (AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0)
	krb5_err (context, 1, errno, "socket AF_UNIX");
    memset (&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strlcpy (addr.sun_path, KADM5_LOG_SIGNAL, sizeof(addr.sun_path));
    unlink (addr.sun_path);
    if (bind (fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	krb5_err (context, 1, errno, "bind %s", addr.sun_path);
    return fd;
}

static int
make_listen_socket (krb5_context context)
{
    int fd;
    int one = 1;
    struct sockaddr_in addr;

    fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
	krb5_err (context, 1, errno, "socket AF_INET");
    setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    memset (&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = krb5_getportbyname (context,
					  IPROP_SERVICE, "tcp", IPROP_PORT);
    if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	krb5_err (context, 1, errno, "bind");
    if (listen(fd, SOMAXCONN) < 0)
	krb5_err (context, 1, errno, "listen");
    return fd;
}

struct slave {
    int fd;
    struct sockaddr_in addr;
    char *name;
    krb5_auth_context ac;
    u_int32_t version;
    struct slave *next;
};

typedef struct slave slave;

static int
check_acl (krb5_context context, const char *name)
{
    FILE *fp;
    char buf[256];
    int ret = 1;

    fp = fopen (KADM5_SLAVE_ACL, "r");
    if (fp == NULL)
	return 1;
    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if (buf[strlen(buf) - 1 ] == '\n')
	    buf[strlen(buf) - 1 ] = '\0';
	if (strcmp (buf, name) == 0) {
	    ret = 0;
	    break;
	}
    }
    fclose (fp);
    return ret;
}

static void
add_slave (krb5_context context, krb5_keytab keytab, slave **root, int fd)
{
    krb5_principal server;
    krb5_error_code ret;
    slave *s;
    socklen_t addr_len;
    krb5_ticket *ticket = NULL;
    char hostname[128];

    s = malloc(sizeof(*s));
    if (s == NULL) {
	krb5_warnx (context, "add_slave: no memory");
	return;
    }
    s->name = NULL;
    s->ac = NULL;

    addr_len = sizeof(s->addr);
    s->fd = accept (fd, (struct sockaddr *)&s->addr, &addr_len);
    if (s->fd < 0) {
	krb5_warn (context, errno, "accept");
	goto error;
    }
    gethostname(hostname, sizeof(hostname));
    ret = krb5_sname_to_principal (context, hostname, IPROP_NAME,
				   KRB5_NT_SRV_HST, &server);
    if (ret) {
	krb5_warn (context, ret, "krb5_sname_to_principal");
	goto error;
    }

    ret = krb5_recvauth (context, &s->ac, &s->fd,
			 IPROP_VERSION, server, 0, keytab, &ticket);
    krb5_free_principal (context, server);
    if (ret) {
	krb5_warn (context, ret, "krb5_recvauth");
	goto error;
    }
    ret = krb5_unparse_name (context, ticket->client, &s->name);
    if (ret) {
	krb5_warn (context, ret, "krb5_unparse_name");
	goto error;
    }
    if (check_acl (context, s->name)) {
	krb5_warnx (context, "%s not in acl", s->name);
	goto error;
    }
    krb5_free_ticket (context, ticket);
    krb5_warnx (context, "connection from %s", s->name);

    s->version = 0;
    s->next = *root;
    *root = s;
    return;
error:
    if (s->name)
	free (s->name);
    if (s->ac)
	krb5_auth_con_free(context, s->ac);
    if (ticket)
    krb5_free_ticket (context, ticket);
    close (s->fd);
    free(s);
}

static void
remove_slave (krb5_context context, slave *s, slave **root)
{
    slave **p;

    close (s->fd);
    free (s->name);
    krb5_auth_con_free (context, s->ac);

    for (p = root; *p; p = &(*p)->next)
	if (*p == s) {
	    *p = s->next;
	    break;
	}
    free (s);
}

struct prop_context {
    krb5_auth_context auth_context;
    int fd;
};

static int
prop_one (krb5_context context, HDB *db, hdb_entry *entry, void *v)
{
    krb5_error_code ret;
    krb5_data data;
    struct slave *slave = (struct slave *)v;

    ret = hdb_entry2value (context, entry, &data);
    if (ret)
	return ret;
    ret = krb5_data_realloc (&data, data.length + 4);
    if (ret) {
	krb5_data_free (&data);
	return ret;
    }
    memmove ((char *)data.data + 4, data.data, data.length - 4);
    _krb5_put_int (data.data, ONE_PRINC, 4);

    ret = krb5_write_priv_message (context, slave->ac, &slave->fd, &data);
    krb5_data_free (&data);
    return ret;
}

static int
send_complete (krb5_context context, slave *s,
	       const char *database, u_int32_t current_version)
{
    krb5_error_code ret;
    HDB *db;
    krb5_data data;
    char buf[8];

    ret = hdb_create (context, &db, database);
    if (ret)
	krb5_err (context, 1, ret, "hdb_create: %s", database);
    ret = db->open (context, db, O_RDONLY, 0);
    if (ret)
	krb5_err (context, 1, ret, "db->open");

    _krb5_put_int(buf, TELL_YOU_EVERYTHING, 4);

    data.data   = buf;
    data.length = 4;

    ret = krb5_write_priv_message(context, s->ac, &s->fd, &data);

    if (ret)
	krb5_err (context, 1, ret, "krb5_write_priv_message");

    ret = hdb_foreach (context, db, 0, prop_one, s);
    if (ret)
	krb5_err (context, 1, ret, "hdb_foreach");

    _krb5_put_int (buf, NOW_YOU_HAVE, 4);
    _krb5_put_int (buf + 4, current_version, 4);
    data.length = 8;

    ret = krb5_write_priv_message(context, s->ac, &s->fd, &data);

    if (ret)
	krb5_err (context, 1, ret, "krb5_write_priv_message");

    return 0;
}

static int
send_diffs (krb5_context context, slave *s, int log_fd,
	    const char *database, u_int32_t current_version)
{
    krb5_storage *sp;
    u_int32_t ver;
    time_t timestamp;
    enum kadm_ops op;
    u_int32_t len;
    off_t right, left;
    krb5_data data;
    int ret = 0;

    if (s->version == current_version)
	return 0;

    sp = kadm5_log_goto_end (log_fd);
    right = sp->seek(sp, 0, SEEK_CUR);
    for (;;) {
	if (kadm5_log_previous (sp, &ver, &timestamp, &op, &len))
	    abort ();
	left = sp->seek(sp, -16, SEEK_CUR);
	if (ver == s->version)
	    return 0;
	if (ver == s->version + 1)
	    break;
	if (left == 0)
	    return send_complete (context, s, database, current_version);
    }
    krb5_data_alloc (&data, right - left + 4);
    sp->fetch (sp, (char *)data.data + 4, data.length - 4);
    krb5_storage_free(sp);

    _krb5_put_int(data.data, FOR_YOU, 4);

    ret = krb5_write_priv_message(context, s->ac, &s->fd, &data);

    if (ret) {
	krb5_warn (context, ret, "krb5_write_priv_message");
	return 1;
    }
    return 0;
}

static int
process_msg (krb5_context context, slave *s, int log_fd,
	     const char *database, u_int32_t current_version)
{
    int ret = 0;
    krb5_data out;
    krb5_storage *sp;
    int32_t tmp;

    ret = krb5_read_priv_message(context, s->ac, &s->fd, &out);
    if(ret) {
	krb5_warn (context, ret, "error reading message from %s", s->name);
	return 1;
    }

    sp = krb5_storage_from_mem (out.data, out.length);
    krb5_ret_int32 (sp, &tmp);
    switch (tmp) {
    case I_HAVE :
	krb5_ret_int32 (sp, &tmp);
	s->version = tmp;
	ret = send_diffs (context, s, log_fd, database, current_version);
	break;
    case FOR_YOU :
    default :
	krb5_warnx (context, "Ignoring command %d", tmp);
	break;
    }

    krb5_data_free (&out);
    return ret;
}

static char *realm;
static int version_flag;
static int help_flag;
static char *keytab_str = "HDB:";
static char *database;

static struct getargs args[] = {
    { "realm", 'r', arg_string, &realm },
    { "keytab", 'k', arg_string, &keytab_str,
      "keytab to get authentication from", "kspec" },
    { "database", 'd', arg_string, &database, "database", "file"},
    { "version", 0, arg_flag, &version_flag },
    { "help", 0, arg_flag, &help_flag }
};
static int num_args = sizeof(args) / sizeof(args[0]);

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    void *kadm_handle;
    kadm5_server_context *server_context;
    kadm5_config_params conf;
    int signal_fd, listen_fd;
    int log_fd;
    slave *slaves = NULL;
    u_int32_t current_version, old_version = 0;
    krb5_keytab keytab;
    int optind;
    
    optind = krb5_program_setup(&context, argc, argv, args, num_args, NULL);
    
    if(help_flag)
	krb5_std_usage(0, args, num_args);
    if(version_flag) {
	print_version(NULL);
	exit(0);
    }

    krb5_openlog (context, "ipropd-master", &log_facility);
    krb5_set_warn_dest(context, log_facility);

    ret = krb5_kt_register(context, &hdb_kt_ops);
    if(ret)
	krb5_err(context, 1, ret, "krb5_kt_register");

    ret = krb5_kt_resolve(context, keytab_str, &keytab);
    if(ret)
	krb5_err(context, 1, ret, "krb5_kt_resolve: %s", keytab_str);
    
    memset(&conf, 0, sizeof(conf));
    if(realm) {
	conf.mask |= KADM5_CONFIG_REALM;
	conf.realm = realm;
    }
    ret = kadm5_init_with_skey_ctx (context,
				    KADM5_ADMIN_SERVICE,
				    NULL,
				    KADM5_ADMIN_SERVICE,
				    &conf, 0, 0, 
				    &kadm_handle);
    if (ret)
	krb5_err (context, 1, ret, "kadm5_init_with_password_ctx");

    server_context = (kadm5_server_context *)kadm_handle;

    log_fd = open (server_context->log_context.log_file, O_RDONLY, 0);
    if (log_fd < 0)
	krb5_err (context, 1, errno, "open %s",
		  server_context->log_context.log_file);

    signal_fd = make_signal_socket (context);
    listen_fd = make_listen_socket (context);

    signal (SIGPIPE, SIG_IGN);

    for (;;) {
	slave *p;
	fd_set readset;
	int max_fd = 0;
	struct timeval to = {30, 0};
	u_int32_t vers;

	if (signal_fd >= FD_SETSIZE || listen_fd >= FD_SETSIZE)
	    krb5_errx (context, 1, "fd too large");

	FD_ZERO(&readset);
	FD_SET(signal_fd, &readset);
	max_fd = max(max_fd, signal_fd);
	FD_SET(listen_fd, &readset);
	max_fd = max(max_fd, listen_fd);

	for (p = slaves; p != NULL; p = p->next) {
	    FD_SET(p->fd, &readset);
	    max_fd = max(max_fd, p->fd);
	}

	ret = select (max_fd + 1,
		      &readset, NULL, NULL, &to);
	if (ret < 0) {
	    if (errno == EINTR)
		continue;
	    else
		krb5_err (context, 1, errno, "select");
	}

	if (ret == 0) {
	    old_version = current_version;
	    kadm5_log_get_version_fd (log_fd, &current_version);

	    if (current_version > old_version)
		for (p = slaves; p != NULL; p = p->next)
		    send_diffs (context, p, log_fd, database, current_version);
	}

	if (ret && FD_ISSET(signal_fd, &readset)) {
	    struct sockaddr_un peer_addr;
	    socklen_t peer_len = sizeof(peer_addr);

	    if(recvfrom(signal_fd, &vers, sizeof(vers), 0,
			(struct sockaddr *)&peer_addr, &peer_len) < 0) {
		krb5_warn (context, errno, "recvfrom");
		continue;
	    }
	    --ret;
	    old_version = current_version;
	    kadm5_log_get_version_fd (log_fd, &current_version);
	    for (p = slaves; p != NULL; p = p->next)
		send_diffs (context, p, log_fd, database, current_version);
	}

	for(p = slaves; p != NULL; p = p->next)
	    if (FD_ISSET(p->fd, &readset)) {
		--ret;
		if(process_msg (context, p, log_fd, database, current_version))
		    remove_slave (context, p, &slaves);
	    }

	if (ret && FD_ISSET(listen_fd, &readset)) {
	    add_slave (context, keytab, &slaves, listen_fd);
	    --ret;
	}

    }

    return 0;
}
