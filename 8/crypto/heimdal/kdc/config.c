/*
 * Copyright (c) 1997-2007 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 *
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

#include "kdc_locl.h"
#include <getarg.h>
#include <parse_bytes.h>

RCSID("$Id: config.c 22248 2007-12-08 23:52:12Z lha $");

struct dbinfo {
    char *realm;
    char *dbname;
    char *mkey_file;
    struct dbinfo *next;
};

static char *config_file;	/* location of kdc config file */

static int require_preauth = -1; /* 1 == require preauth for all principals */
static char *max_request_str;	/* `max_request' as a string */

static int disable_des = -1;
static int enable_v4 = -1;
static int enable_kaserver = -1;
static int enable_524 = -1;
static int enable_v4_cross_realm = -1;

static int builtin_hdb_flag;
static int help_flag;
static int version_flag;

static struct getarg_strings addresses_str;	/* addresses to listen on */

static char *v4_realm;

static struct getargs args[] = {
    { 
	"config-file",	'c',	arg_string,	&config_file, 
	"location of config file",	"file" 
    },
    { 
	"require-preauth",	'p',	arg_negative_flag, &require_preauth, 
	"don't require pa-data in as-reqs"
    },
    { 
	"max-request",	0,	arg_string, &max_request, 
	"max size for a kdc-request", "size"
    },
    { "enable-http", 'H', arg_flag, &enable_http, "turn on HTTP support" },
    {	"524",		0, 	arg_negative_flag, &enable_524,
	"don't respond to 524 requests" 
    },
    {
	"kaserver", 'K', arg_flag,   &enable_kaserver,
	"enable kaserver support"
    },
    {	"kerberos4",	0, 	arg_flag, &enable_v4,
	"respond to kerberos 4 requests" 
    },
    { 
	"v4-realm",	'r',	arg_string, &v4_realm, 
	"realm to serve v4-requests for"
    },
    {	"kerberos4-cross-realm",	0, 	arg_flag,
	&enable_v4_cross_realm,
	"respond to kerberos 4 requests from foreign realms" 
    },
    {	"ports",	'P', 	arg_string, &port_str,
	"ports to listen to", "portspec"
    },
#if DETACH_IS_DEFAULT
    {
	"detach",       'D',      arg_negative_flag, &detach_from_console, 
	"don't detach from console"
    },
#else
    {
	"detach",       0 ,      arg_flag, &detach_from_console, 
	"detach from console"
    },
#endif
    {	"addresses",	0,	arg_strings, &addresses_str,
	"addresses to listen on", "list of addresses" },
    {	"disable-des",	0,	arg_flag, &disable_des,
	"disable DES" },
    {	"builtin-hdb",	0,	arg_flag,   &builtin_hdb_flag,
	"list builtin hdb backends"},
    {	"help",		'h',	arg_flag,   &help_flag },
    {	"version",	'v',	arg_flag,   &version_flag }
};

static int num_args = sizeof(args) / sizeof(args[0]);

static void
usage(int ret)
{
    arg_printusage (args, num_args, NULL, "");
    exit (ret);
}

static void
add_one_address (krb5_context context, const char *str, int first)
{
    krb5_error_code ret;
    krb5_addresses tmp;

    ret = krb5_parse_address (context, str, &tmp);
    if (ret)
	krb5_err (context, 1, ret, "parse_address `%s'", str);
    if (first)
	krb5_copy_addresses(context, &tmp, &explicit_addresses);
    else
	krb5_append_addresses(context, &explicit_addresses, &tmp);
    krb5_free_addresses (context, &tmp);
}

krb5_kdc_configuration *
configure(krb5_context context, int argc, char **argv)
{
    krb5_kdc_configuration *config;
    krb5_error_code ret;
    int optidx = 0;
    const char *p;
    
    while(getarg(args, num_args, argc, argv, &optidx))
	warnx("error at argument `%s'", argv[optidx]);

    if(help_flag)
	usage (0);

    if (version_flag) {
	print_version(NULL);
	exit(0);
    }

    if (builtin_hdb_flag) {
	char *list;
	ret = hdb_list_builtin(context, &list);
	if (ret)
	    krb5_err(context, 1, ret, "listing builtin hdb backends");
	printf("builtin hdb backends: %s\n", list);
	free(list);
	exit(0);
    }

    argc -= optidx;
    argv += optidx;

    if (argc != 0)
	usage(1);
    
    {
	char **files;

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
	if(ret) 
	    krb5_err(context, 1, ret, "reading configuration files");
    }

    ret = krb5_kdc_get_config(context, &config);
    if (ret)
	krb5_err(context, 1, ret, "krb5_kdc_default_config");

    kdc_openlog(context, config);

    ret = krb5_kdc_set_dbinfo(context, config);
    if (ret)
	krb5_err(context, 1, ret, "krb5_kdc_set_dbinfo");

    if(max_request_str)
	max_request = parse_bytes(max_request_str, NULL);

    if(max_request == 0){
	p = krb5_config_get_string (context,
				    NULL,
				    "kdc",
				    "max-request",
				    NULL);
	if(p)
	    max_request = parse_bytes(p, NULL);
    }
    
    if(require_preauth != -1)
	config->require_preauth = require_preauth;

    if(port_str == NULL){
	p = krb5_config_get_string(context, NULL, "kdc", "ports", NULL);
	if (p != NULL)
	    port_str = strdup(p);
    }

    explicit_addresses.len = 0;

    if (addresses_str.num_strings) {
	int i;

	for (i = 0; i < addresses_str.num_strings; ++i)
	    add_one_address (context, addresses_str.strings[i], i == 0);
	free_getarg_strings (&addresses_str);
    } else {
	char **foo = krb5_config_get_strings (context, NULL,
					      "kdc", "addresses", NULL);

	if (foo != NULL) {
	    add_one_address (context, *foo++, TRUE);
	    while (*foo)
		add_one_address (context, *foo++, FALSE);
	}
    }

    if(enable_v4 != -1)
	config->enable_v4 = enable_v4;

    if(enable_v4_cross_realm != -1)
	config->enable_v4_cross_realm = enable_v4_cross_realm;

    if(enable_524 != -1)
	config->enable_524 = enable_524;

    if(enable_http == -1)
	enable_http = krb5_config_get_bool(context, NULL, "kdc", 
					   "enable-http", NULL);

    if(request_log == NULL)
	request_log = krb5_config_get_string(context, NULL, 
					     "kdc", 
					     "kdc-request-log", 
					     NULL);

    if (krb5_config_get_string(context, NULL, "kdc", 
			       "enforce-transited-policy", NULL))
	krb5_errx(context, 1, "enforce-transited-policy deprecated, "
		  "use [kdc]transited-policy instead");

    if (enable_kaserver != -1)
	config->enable_kaserver = enable_kaserver;

    if(detach_from_console == -1) 
	detach_from_console = krb5_config_get_bool_default(context, NULL, 
							   DETACH_IS_DEFAULT,
							   "kdc",
							   "detach", NULL);

    if(max_request == 0)
	max_request = 64 * 1024;

    if (port_str == NULL)
	port_str = "+";

    if (v4_realm)
	config->v4_realm = v4_realm;

    if(config->v4_realm == NULL && (config->enable_kaserver || config->enable_v4))
	krb5_errx(context, 1, "Kerberos 4 enabled but no realm configured");

    if(disable_des == -1)
	disable_des = krb5_config_get_bool_default(context, NULL, 
						   FALSE,
						   "kdc",
						   "disable-des", NULL);
    if(disable_des) {
	krb5_enctype_disable(context, ETYPE_DES_CBC_CRC);
	krb5_enctype_disable(context, ETYPE_DES_CBC_MD4);
	krb5_enctype_disable(context, ETYPE_DES_CBC_MD5);
	krb5_enctype_disable(context, ETYPE_DES_CBC_NONE);
	krb5_enctype_disable(context, ETYPE_DES_CFB64_NONE);
	krb5_enctype_disable(context, ETYPE_DES_PCBC_NONE);

	kdc_log(context, config, 
		0, "DES was disabled, turned off Kerberos V4, 524 "
		"and kaserver");
	config->enable_v4 = 0;
	config->enable_524 = 0;
	config->enable_kaserver = 0;
    }

    krb5_kdc_windc_init(context);

    return config;
}
