/*
 * Copyright (c) 1997-2000 Kungliga Tekniska Högskolan
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

#include "kadmin_locl.h"

RCSID("$Id: kadmind.c,v 1.24 2000/12/31 07:45:23 assar Exp $");

static char *check_library  = NULL;
static char *check_function = NULL;
static char *config_file;
static char *keyfile;
static char *keytab_str = "HDB:";
static int help_flag;
static int version_flag;
static int debug_flag;
static char *port_str;
char *realm;

static struct getargs args[] = {
    { 
	"config-file",	'c',	arg_string,	&config_file, 
	"location of config file",	"file" 
    },
    {
	"key-file",	'k',	arg_string, &keyfile, 
	"location of master key file", "file"
    },
    {
	"keytab",	0,	arg_string, &keytab_str,
	"what keytab to use", "keytab"
    },
    {	"realm",	'r',	arg_string,   &realm, 
	"realm to use", "realm" 
    },
#ifdef HAVE_DLOPEN
    { "check-library", 0, arg_string, &check_library, 
      "library to load password check function from", "library" },
    { "check-function", 0, arg_string, &check_function,
      "password check function to load", "function" },
#endif
    {	"debug",	'd',	arg_flag,   &debug_flag, 
	"enable debugging" 
    },
    {	"ports",	'p',	arg_string, &port_str, 
	"ports to listen to", "port" },
    {	"help",		'h',	arg_flag,   &help_flag },
    {	"version",	'v',	arg_flag,   &version_flag }
};

static int num_args = sizeof(args) / sizeof(args[0]);

krb5_context context;

static void
usage(int ret)
{
    arg_printusage (args, num_args, NULL, "");
    exit (ret);
}

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_config_section *cf;
    int optind = 0;
    int e;
    krb5_log_facility *logf;
    krb5_keytab keytab;

    set_progname(argv[0]);

    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    ret = krb5_openlog(context, "kadmind", &logf);
    ret = krb5_set_warn_dest(context, logf);

    while((e = getarg(args, num_args, argc, argv, &optind)))
	warnx("error at argument `%s'", argv[optind]);

    if (help_flag)
	usage (0);

    if (version_flag) {
	print_version(NULL);
	exit(0);
    }

    argc -= optind;
    argv += optind;

    ret = krb5_kt_register(context, &hdb_kt_ops);
    if(ret)
	krb5_err(context, 1, ret, "krb5_kt_register");

    if (config_file == NULL)
	config_file = HDB_DB_DIR "/kdc.conf";

    if(krb5_config_parse_file(config_file, &cf) == 0) {
	const char *p = krb5_config_get_string (context, cf, 
						"kdc", "key-file", NULL);
	if (p)
	    keyfile = strdup(p);
    }

    ret = krb5_kt_resolve(context, keytab_str, &keytab);
    if(ret)
	krb5_err(context, 1, ret, "krb5_kt_resolve");

    kadm5_setup_passwd_quality_check (context, check_library, check_function);

    {
	int fd = 0;
	struct sockaddr sa;
	socklen_t sa_size;
	krb5_auth_context ac = NULL;
	int debug_port;
	sa_size = sizeof(sa);
	if(debug_flag) {
	    if(port_str == NULL)
		debug_port = krb5_getportbyname (context, "kerberos-adm", 
						 "tcp", 749);
	    else
		debug_port = htons(atoi(port_str));
	    mini_inetd(debug_port);
	} else if(roken_getsockname(STDIN_FILENO, &sa, &sa_size) < 0 && 
		   errno == ENOTSOCK) {
	    parse_ports(context, port_str ? port_str : "+");
	    pidfile(NULL);
	    start_server(context);
	}
	if(realm)
	    krb5_set_default_realm(context, realm); /* XXX */
	kadmind_loop(context, ac, keytab, fd);
    }
    return 0;
}
