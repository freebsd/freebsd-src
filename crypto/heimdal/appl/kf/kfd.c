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

#include "kf_locl.h"
RCSID("$Id: kfd.c,v 1.8 2001/01/09 18:43:10 assar Exp $");

krb5_context context;
char krb5_tkfile[MAXPATHLEN];

static int help_flag;
static int version_flag;
static char *port_str;
char *service = SERVICE;
int do_inetd = 0;
static char *regpag_str=NULL;

static struct getargs args[] = {
    { "port", 'p', arg_string, &port_str, "port to listen to", "port" },
    { "inetd",'i',arg_flag, &do_inetd,
       "Not started from inetd", NULL },
    { "regpag",'R',arg_string,&regpag_str,"path to regpag binary","regpag"},
    { "help", 'h', arg_flag, &help_flag },
    { "version", 0, arg_flag, &version_flag }
};

static int num_args = sizeof(args) / sizeof(args[0]);

static void
usage(int code, struct getargs *args, int num_args)
{
    arg_printusage(args, num_args, NULL, "");
    exit(code);
}

static int
server_setup(krb5_context *context, int argc, char **argv)
{
    int port = 0;
    int local_argc;

    local_argc = krb5_program_setup(context, argc, argv, args, num_args, usage);

    if(help_flag)
	(*usage)(0, args, num_args);
    if(version_flag) {
	print_version(NULL);
	exit(0);
    }
    
    if(port_str){
	struct servent *s = roken_getservbyname(port_str, "tcp");
	if(s)
	    port = s->s_port;
	else {
	    char *ptr;

	    port = strtol (port_str, &ptr, 10);
	    if (port == 0 && ptr == port_str)
		errx (1, "Bad port `%s'", port_str);
	    port = htons(port);
	}
    }

    if (port == 0)
	port = krb5_getportbyname (*context, PORT, "tcp", PORT_NUM);

    if(argv[local_argc] != NULL)
        usage(1, args, num_args);
    
    return port;
}

static void
syslog_and_die (const char *m, ...)
{
    va_list args;

    va_start(args, m);
    vsyslog (LOG_ERR, m, args);
    va_end(args);
    exit (1);
}

static void
syslog_and_cont (const char *m, ...)
{
    va_list args;

    va_start(args, m);
    vsyslog (LOG_ERR, m, args);
    va_end(args);
    return; 
}

static int
proto (int sock, const char *service)
{
    krb5_auth_context auth_context;
    krb5_error_code status;
    krb5_principal server;
    krb5_ticket *ticket;
    char *name;
    char ret_string[10];
    char hostname[MAXHOSTNAMELEN];
    krb5_data packet;
    krb5_data data;
    krb5_data remotename;
    krb5_data tk_file;

    u_int32_t len, net_len;
    krb5_ccache ccache;
    char ccname[MAXPATHLEN];
    struct passwd *pwd;
    ssize_t n;

    status = krb5_auth_con_init (context, &auth_context);
    if (status)
	syslog_and_die("krb5_auth_con_init: %s",
	      krb5_get_err_text(context, status));

    status = krb5_auth_con_setaddrs_from_fd (context,
					     auth_context,
					     &sock);
    if (status)
	syslog_and_die("krb5_auth_con_setaddr: %s",
	      krb5_get_err_text(context, status));

    if(gethostname (hostname, sizeof(hostname)) < 0)
	syslog_and_die("gethostname: %s",strerror(errno));

    status = krb5_sname_to_principal (context,
				      hostname,
				      service,
				      KRB5_NT_SRV_HST,
				      &server);
    if (status)
	syslog_and_die("krb5_sname_to_principal: %s",
	      krb5_get_err_text(context, status));

    status = krb5_recvauth (context,
			    &auth_context,
			    &sock,
			    VERSION,
			    server,
			    0,
			    NULL,
			    &ticket);
    if (status)
	syslog_and_die("krb5_recvauth: %s",
	      krb5_get_err_text(context, status));

    status = krb5_unparse_name (context,
				ticket->client,
				&name);
    if (status)
	syslog_and_die("krb5_unparse_name: %s",
	      krb5_get_err_text(context, status));

    status=krb5_read_message (context, &sock, &remotename);
    if (status) {
	syslog_and_die("krb5_read_message: %s",
		       krb5_get_err_text(context, status));
    }
    status=krb5_read_message (context, &sock, &tk_file);
    if (status) {
	syslog_and_die("krb5_read_message: %s",
		       krb5_get_err_text(context, status));
    }

    krb5_data_zero (&data);
    krb5_data_zero (&packet);

    n = krb5_net_read (context, &sock, &net_len, 4);
    if (n < 0)
        syslog_and_die("krb5_net_read: %s", strerror(errno));
    if (n == 0)
        syslog_and_die("EOF in krb5_net_read");

    len = ntohl(net_len);
    krb5_data_alloc (&packet, len);
    n = krb5_net_read (context, &sock, packet.data, len);
    if (n < 0)
        syslog_and_die("krb5_net_read: %s", strerror(errno));
    if (n == 0)
        syslog_and_die("EOF in krb5_net_read");

    status = krb5_rd_priv (context,
                           auth_context,
                           &packet,
                           &data,
                           NULL);
    if (status) {
	syslog_and_cont("krb5_rd_priv: %s",
			krb5_get_err_text(context, status));
	goto out;
    }

    pwd = getpwnam ((char *)(remotename.data));
    if (pwd == NULL) {
	status=1;
	syslog_and_cont("getpwnam: %s failed",(char *)(remotename.data));
	goto out;
    }

    if(!krb5_kuserok (context,
                     ticket->client,
                     (char *)(remotename.data))) {
	status=1;
	syslog_and_cont("krb5_kuserok: permission denied");
	goto out;
    }

    if (setgid(pwd->pw_gid) < 0) {
	syslog_and_cont ("setgid: %s", strerror(errno));
	goto out;
    }
    if (setuid(pwd->pw_uid) < 0) {
	syslog_and_cont ("setuid: %s", strerror(errno));
	goto out;
    }

    if (tk_file.length != 1)
	snprintf (ccname, sizeof(ccname), "%s", (char *)(tk_file.data));
    else
	snprintf (ccname, sizeof(ccname), "FILE:/tmp/krb5cc_%u",pwd->pw_uid);

    status = krb5_cc_resolve (context, ccname, &ccache);
    if (status) {
	syslog_and_cont("krb5_cc_resolve: %s",
			krb5_get_err_text(context, status));
        goto out;
    }
    status = krb5_cc_initialize (context, ccache, ticket->client);
    if (status) {
	syslog_and_cont("krb5_cc_initialize: %s",
			krb5_get_err_text(context, status));
        goto out;
    }
    status = krb5_rd_cred2 (context, auth_context, ccache, &data);
    krb5_cc_close (context, ccache);
    if (status) {
	syslog_and_cont("krb5_rd_cred: %s",
			krb5_get_err_text(context, status));
        goto out;

    }
    strlcpy(krb5_tkfile,ccname,sizeof(krb5_tkfile));
    syslog_and_cont("%s forwarded ticket to %s,%s",
		    name,
		    (char *)(remotename.data),ccname);
out:
    if (status) {
	strcpy(ret_string, "no");
	syslog_and_cont("failed");
    } else  {
	strcpy(ret_string, "ok");
    }

    krb5_data_free (&tk_file);
    krb5_data_free (&remotename);
    krb5_data_free (&packet);
    krb5_data_free (&data);
    free(name);

    len = strlen(ret_string) + 1;
    net_len = htonl(len);
    if (krb5_net_write (context, &sock, &net_len, 4) != 4)
         return 1;
    if (krb5_net_write (context, &sock, ret_string, len) != len)
         return 1;
    return status;
}

static int
doit (int port, const char *service)
{
    if (do_inetd)
	mini_inetd(port);
    return proto (STDIN_FILENO, service);
}

int
main(int argc, char **argv)
{
    int port;
    int ret;

    set_progname (argv[0]);
    roken_openlog (argv[0], LOG_ODELAY | LOG_PID,LOG_AUTH);
    port = server_setup(&context, argc, argv);
    ret = doit (port, service);
    closelog();
    if (ret == 0 && regpag_str != NULL)
        ret = execl(regpag_str, "regpag", "-t", krb5_tkfile, "-r", NULL);
    return ret;
}
