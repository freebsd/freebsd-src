/*
 * Copyright (c) 1997-2001 Kungliga Tekniska Högskolan
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

#include "kpasswd_locl.h"
RCSID("$Id: kpasswdd.c,v 1.49 2001/01/11 21:33:53 assar Exp $");

#include <kadm5/admin.h>

#include <hdb.h>

static krb5_context context;
static krb5_log_facility *log_facility;

static sig_atomic_t exit_flag = 0;

static void
send_reply (int s,
	    struct sockaddr *sa,
	    int sa_size,
	    krb5_data *ap_rep,
	    krb5_data *rest)
{
    struct msghdr msghdr;
    struct iovec iov[3];
    u_int16_t len, ap_rep_len;
    u_char header[6];
    u_char *p;

    if (ap_rep)
	ap_rep_len = ap_rep->length;
    else
	ap_rep_len = 0;

    len = 6 + ap_rep_len + rest->length;
    p = header;
    *p++ = (len >> 8) & 0xFF;
    *p++ = (len >> 0) & 0xFF;
    *p++ = 0;
    *p++ = 1;
    *p++ = (ap_rep_len >> 8) & 0xFF;
    *p++ = (ap_rep_len >> 0) & 0xFF;

    memset (&msghdr, 0, sizeof(msghdr));
    msghdr.msg_name       = (void *)sa;
    msghdr.msg_namelen    = sa_size;
    msghdr.msg_iov        = iov;
    msghdr.msg_iovlen     = sizeof(iov)/sizeof(*iov);
#if 0
    msghdr.msg_control    = NULL;
    msghdr.msg_controllen = 0;
#endif

    iov[0].iov_base       = (char *)header;
    iov[0].iov_len        = 6;
    if (ap_rep_len) {
	iov[1].iov_base   = ap_rep->data;
	iov[1].iov_len    = ap_rep->length;
    } else {
	iov[1].iov_base   = NULL;
	iov[1].iov_len    = 0;
    }
    iov[2].iov_base       = rest->data;
    iov[2].iov_len        = rest->length;

    if (sendmsg (s, &msghdr, 0) < 0)
	krb5_warn (context, errno, "sendmsg");
}

static int
make_result (krb5_data *data,
	     u_int16_t result_code,
	     const char *expl)
{
    krb5_data_zero (data);

    data->length = asprintf ((char **)&data->data,
			     "%c%c%s",
			     (result_code >> 8) & 0xFF,
			     result_code & 0xFF,
			     expl);

    if (data->data == NULL) {
	krb5_warnx (context, "Out of memory generating error reply");
	return 1;
    }
    return 0;
}

static void
reply_error (krb5_principal server,
	     int s,
	     struct sockaddr *sa,
	     int sa_size,
	     krb5_error_code error_code,
	     u_int16_t result_code,
	     const char *expl)
{
    krb5_error_code ret;
    krb5_data error_data;
    krb5_data e_data;

    if (make_result(&e_data, result_code, expl))
	return;

    ret = krb5_mk_error (context,
			 error_code,
			 NULL,
			 &e_data,
			 NULL,
			 server,
			 0,
			 &error_data);
    krb5_data_free (&e_data);
    if (ret) {
	krb5_warn (context, ret, "Could not even generate error reply");
	return;
    }
    send_reply (s, sa, sa_size, NULL, &error_data);
    krb5_data_free (&error_data);
}

static void
reply_priv (krb5_auth_context auth_context,
	    int s,
	    struct sockaddr *sa,
	    int sa_size,
	    u_int16_t result_code,
	    const char *expl)
{
    krb5_error_code ret;
    krb5_data krb_priv_data;
    krb5_data ap_rep_data;
    krb5_data e_data;

    ret = krb5_mk_rep (context,
		       auth_context,
		       &ap_rep_data);
    if (ret) {
	krb5_warn (context, ret, "Could not even generate error reply");
	return;
    }

    if (make_result(&e_data, result_code, expl))
	return;

    ret = krb5_mk_priv (context,
			auth_context,
			&e_data,
			&krb_priv_data,
			NULL);
    krb5_data_free (&e_data);
    if (ret) {
	krb5_warn (context, ret, "Could not even generate error reply");
	return;
    }
    send_reply (s, sa, sa_size, &ap_rep_data, &krb_priv_data);
    krb5_data_free (&ap_rep_data);
    krb5_data_free (&krb_priv_data);
}

/*
 * Change the password for `principal', sending the reply back on `s'
 * (`sa', `sa_size') to `pwd_data'.
 */

static void
change (krb5_auth_context auth_context,
	krb5_principal principal,
	int s,
	struct sockaddr *sa,
	int sa_size,
	krb5_data *pwd_data)
{
    krb5_error_code ret;
    char *client;
    const char *pwd_reason;
    kadm5_config_params conf;
    void *kadm5_handle;
    char *tmp;

    memset (&conf, 0, sizeof(conf));
    
    krb5_unparse_name (context, principal, &client);

    ret = kadm5_init_with_password_ctx(context, 
				       client,
				       NULL,
				       KADM5_ADMIN_SERVICE,
				       &conf, 0, 0, 
				       &kadm5_handle);
    if (ret) {
	free (client);
	krb5_warn (context, ret, "kadm5_init_with_password_ctx");
	reply_priv (auth_context, s, sa, sa_size, 2,
		    "Internal error");
	return;
    }

    krb5_warnx (context, "Changing password for %s", client);
    free (client);

    pwd_reason = kadm5_check_password_quality (context, principal, pwd_data);
    if (pwd_reason != NULL ) {
	krb5_warnx (context, "%s", pwd_reason);
	reply_priv (auth_context, s, sa, sa_size, 4, pwd_reason);
	kadm5_destroy (kadm5_handle);
	return;
    }

    tmp = malloc (pwd_data->length + 1);
    if (tmp == NULL) {
	krb5_warnx (context, "malloc: out of memory");
	reply_priv (auth_context, s, sa, sa_size, 2,
		    "Internal error");
	goto out;
    }
    memcpy (tmp, pwd_data->data, pwd_data->length);
    tmp[pwd_data->length] = '\0';

    ret = kadm5_s_chpass_principal_cond (kadm5_handle, principal, tmp);
    memset (tmp, 0, pwd_data->length);
    free (tmp);
    if (ret) {
	krb5_warn (context, ret, "kadm5_s_chpass_principal_cond");
	reply_priv (auth_context, s, sa, sa_size, 2,
		    "Internal error");
	goto out;
    }
    reply_priv (auth_context, s, sa, sa_size, 0, "Password changed");
out:
    kadm5_destroy (kadm5_handle);
}

static int
verify (krb5_auth_context *auth_context,
	krb5_principal server,
	krb5_keytab keytab,
	krb5_ticket **ticket,
	krb5_data *out_data,
	int s,
	struct sockaddr *sa,
	int sa_size,
	u_char *msg,
	size_t len)
{
    krb5_error_code ret;
    u_int16_t pkt_len, pkt_ver, ap_req_len;
    krb5_data ap_req_data;
    krb5_data krb_priv_data;

    pkt_len = (msg[0] << 8) | (msg[1]);
    pkt_ver = (msg[2] << 8) | (msg[3]);
    ap_req_len = (msg[4] << 8) | (msg[5]);
    if (pkt_len != len) {
	krb5_warnx (context, "Strange len: %ld != %ld", 
		    (long)pkt_len, (long)len);
	reply_error (server, s, sa, sa_size, 0, 1, "Bad request");
	return 1;
    }
    if (pkt_ver != 0x0001) {
	krb5_warnx (context, "Bad version (%d)", pkt_ver);
	reply_error (server, s, sa, sa_size, 0, 1, "Wrong program version");
	return 1;
    }

    ap_req_data.data   = msg + 6;
    ap_req_data.length = ap_req_len;

    ret = krb5_rd_req (context,
		       auth_context,
		       &ap_req_data,
		       server,
		       keytab,
		       NULL,
		       ticket);
    if (ret) {
	if(ret == KRB5_KT_NOTFOUND) {
	    char *name;
	    krb5_unparse_name(context, server, &name);
	    krb5_warnx (context, "krb5_rd_req: %s (%s)", 
			krb5_get_err_text(context, ret), name);
	    free(name);
	} else
	    krb5_warn (context, ret, "krb5_rd_req");
	reply_error (server, s, sa, sa_size, ret, 3, "Authentication failed");
	return 1;
    }

    if (!(*ticket)->ticket.flags.initial) {
	krb5_warnx (context, "initial flag not set");
	reply_error (server, s, sa, sa_size, ret, 1,
		     "Bad request");
	goto out;
    }
    krb_priv_data.data   = msg + 6 + ap_req_len;
    krb_priv_data.length = len - 6 - ap_req_len;

    ret = krb5_rd_priv (context,
			*auth_context,
			&krb_priv_data,
			out_data,
			NULL);
    
    if (ret) {
	krb5_warn (context, ret, "krb5_rd_priv");
	reply_error (server, s, sa, sa_size, ret, 3, "Bad request");
	goto out;
    }
    return 0;
out:
    krb5_free_ticket (context, *ticket);
    return 1;
}

static void
process (krb5_principal server,
	 krb5_keytab keytab,
	 int s,
	 krb5_address *this_addr,
	 struct sockaddr *sa,
	 int sa_size,
	 u_char *msg,
	 int len)
{
    krb5_error_code ret;
    krb5_auth_context auth_context = NULL;
    krb5_data out_data;
    krb5_ticket *ticket;
    krb5_address other_addr;

    krb5_data_zero (&out_data);

    ret = krb5_auth_con_init (context, &auth_context);
    if (ret) {
	krb5_warn (context, ret, "krb5_auth_con_init");
	return;
    }

    ret = krb5_sockaddr2address (sa, &other_addr);
    if (ret) {
	krb5_warn (context, ret, "krb5_sockaddr2address");
	goto out;
    }

    ret = krb5_auth_con_setaddrs (context,
				  auth_context,
				  this_addr,
				  &other_addr);
    krb5_free_address (context, &other_addr);
    if (ret) {
	krb5_warn (context, ret, "krb5_auth_con_setaddr");
	goto out;
    }

    if (verify (&auth_context, server, keytab, &ticket, &out_data,
		s, sa, sa_size, msg, len) == 0) {
	change (auth_context,
		ticket->client,
		s,
		sa, sa_size,
		&out_data);
	memset (out_data.data, 0, out_data.length);
	krb5_free_ticket (context, ticket);
	free (ticket);
    }

out:
    krb5_data_free (&out_data);
    krb5_auth_con_free (context, auth_context);
}

static int
doit (krb5_keytab keytab, int port)
{
    krb5_error_code ret;
    krb5_principal server;
    int *sockets;
    int maxfd;
    char *realm;
    krb5_addresses addrs;
    unsigned n, i;
    fd_set real_fdset;
    struct sockaddr_storage __ss;
    struct sockaddr *sa = (struct sockaddr *)&__ss;

    ret = krb5_get_default_realm (context, &realm);
    if (ret)
	krb5_err (context, 1, ret, "krb5_get_default_realm");

    ret = krb5_build_principal (context,
				&server,
				strlen(realm),
				realm,
				"kadmin",
				"changepw",
				NULL);
    if (ret)
	krb5_err (context, 1, ret, "krb5_build_principal");

    free (realm);

    ret = krb5_get_all_server_addrs (context, &addrs);
    if (ret)
	krb5_err (context, 1, ret, "krb5_get_all_server_addrs");

    n = addrs.len;

    sockets = malloc (n * sizeof(*sockets));
    if (sockets == NULL)
	krb5_errx (context, 1, "out of memory");
    maxfd = 0;
    FD_ZERO(&real_fdset);
    for (i = 0; i < n; ++i) {
	int sa_size;

	krb5_addr2sockaddr (&addrs.val[i], sa, &sa_size, port);

	
	sockets[i] = socket (sa->sa_family, SOCK_DGRAM, 0);
	if (sockets[i] < 0)
	    krb5_err (context, 1, errno, "socket");
	if (bind (sockets[i], sa, sa_size) < 0) {
	    char str[128];
	    size_t len;
	    ret = krb5_print_address (&addrs.val[i], str, sizeof(str), &len);
	    krb5_err (context, 1, errno, "bind(%s)", str);
	}
	maxfd = max (maxfd, sockets[i]);
	if (maxfd >= FD_SETSIZE)
	    krb5_errx (context, 1, "fd too large");
	FD_SET(sockets[i], &real_fdset);
    }

    while(exit_flag == 0) {
	int ret;
	fd_set fdset = real_fdset;

	ret = select (maxfd + 1, &fdset, NULL, NULL, NULL);
	if (ret < 0) {
	    if (errno == EINTR)
		continue;
	    else
		krb5_err (context, 1, errno, "select");
	}
	for (i = 0; i < n; ++i)
	    if (FD_ISSET(sockets[i], &fdset)) {
		u_char buf[BUFSIZ];
		socklen_t addrlen = sizeof(__ss);

		ret = recvfrom (sockets[i], buf, sizeof(buf), 0,
				sa, &addrlen);
		if (ret < 0) {
		    if(errno == EINTR)
			break;
		    else
			krb5_err (context, 1, errno, "recvfrom");
		}

		process (server, keytab, sockets[i],
			 &addrs.val[i],
			 sa, addrlen,
			 buf, ret);
	    }
    }
    krb5_free_addresses (context, &addrs);
    krb5_free_principal (context, server);
    krb5_free_context (context);
    return 0;
}

static RETSIGTYPE
sigterm(int sig)
{
    exit_flag = 1;
}

const char *check_library  = NULL;
const char *check_function = NULL;
char *keytab_str = "HDB:";
char *realm_str;
int version_flag;
int help_flag;
char *port_str;

struct getargs args[] = {
#ifdef HAVE_DLOPEN
    { "check-library", 0, arg_string, &check_library, 
      "library to load password check function from", "library" },
    { "check-function", 0, arg_string, &check_function,
      "password check function to load", "function" },
#endif
    { "keytab", 'k', arg_string, &keytab_str, 
      "keytab to get authentication key from", "kspec" },
    { "realm", 'r', arg_string, &realm_str, "default realm", "realm" },
    { "port",  'p', arg_string, &port_str, "port" },
    { "version", 0, arg_flag, &version_flag },
    { "help", 0, arg_flag, &help_flag }
};
int num_args = sizeof(args) / sizeof(args[0]);

int
main (int argc, char **argv)
{
    int optind;
    krb5_keytab keytab;
    krb5_error_code ret;
    int port;
    
    optind = krb5_program_setup(&context, argc, argv, args, num_args, NULL);
    
    if(help_flag)
	krb5_std_usage(0, args, num_args);
    if(version_flag) {
	print_version(NULL);
	exit(0);
    }

    if(realm_str)
	krb5_set_default_realm(context, realm_str);
    
    krb5_openlog (context, "kpasswdd", &log_facility);
    krb5_set_warn_dest(context, log_facility);

    if (port_str != NULL) {
	struct servent *s = roken_getservbyname (port_str, "udp");

	if (s != NULL)
	    port = s->s_port;
	else {
	    char *ptr;

	    port = strtol (port_str, &ptr, 10);
	    if (port == 0 && ptr == port_str)
		krb5_errx (context, 1, "bad port `%s'", port_str);
	    port = htons(port);
	}
    } else
	port = krb5_getportbyname (context, "kpasswd", "udp", KPASSWD_PORT);

    ret = krb5_kt_register(context, &hdb_kt_ops);
    if(ret)
	krb5_err(context, 1, ret, "krb5_kt_register");

    ret = krb5_kt_resolve(context, keytab_str, &keytab);
    if(ret)
	krb5_err(context, 1, ret, "%s", keytab_str);
    
    kadm5_setup_passwd_quality_check (context, check_library, check_function);

#ifdef HAVE_SIGACTION
    {
	struct sigaction sa;

	sa.sa_flags = 0;
	sa.sa_handler = sigterm;
	sigemptyset(&sa.sa_mask);

	sigaction(SIGINT,  &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
    }
#else
    signal(SIGINT,  sigterm);
    signal(SIGTERM, sigterm);
#endif

    pidfile(NULL);

    return doit (keytab, port);
}
