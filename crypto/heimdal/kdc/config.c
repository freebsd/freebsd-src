/*
 * Copyright (c) 1997-1999 Kungliga Tekniska Högskolan
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

#include "kdc_locl.h"
#include <getarg.h>
#include <parse_bytes.h>

RCSID("$Id: config.c,v 1.28 1999/12/02 17:04:58 joda Exp $");

static char *config_file;
int require_preauth = -1;
char *keyfile;
static char *max_request_str;
size_t max_request;
time_t kdc_warn_pwexpire;
struct dbinfo *databases;
HDB **db;
int num_db;
char *port_str;
int enable_http = -1;
krb5_boolean encode_as_rep_as_tgs_rep; /* bug compatibility */

krb5_boolean check_ticket_addresses;
krb5_boolean allow_null_ticket_addresses;

#ifdef KRB4
char *v4_realm;
#endif
#ifdef KASERVER
krb5_boolean enable_kaserver = -1;
#endif

static int help_flag;
static int version_flag;

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
	"key-file",	'k',	arg_string, &keyfile, 
	"location of master key file", "file"
    },
    { 
	"max-request",	0,	arg_string, &max_request, 
	"max size for a kdc-request", "size"
    },
#if 0
    {
	"database",	'd', 	arg_string, &databases,
	"location of database", "database"
    },
#endif
    { "enable-http", 'H', arg_flag, &enable_http, "turn on HTTP support" },
#ifdef KRB4
    { 
	"v4-realm",	'r',	arg_string, &v4_realm, 
	"realm to serve v4-requests for"
    },
#endif
#ifdef KASERVER
    {
	"kaserver", 'K', arg_negative_flag,   &enable_kaserver,
	"turn off kaserver support"
    },
#endif
    {	"ports",	'P', 	arg_string, &port_str,
	"ports to listen to" 
    },
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
get_dbinfo(krb5_config_section *cf)
{
    krb5_config_binding *top_binding = NULL;
    krb5_config_binding *db_binding;
    krb5_config_binding *default_binding = NULL;
    struct dbinfo *di, **dt;
    const char *default_dbname = HDB_DEFAULT_DB;
    const char *default_mkey = HDB_DB_DIR "/m-key";
    const char *p;

    databases = NULL;
    dt = &databases;
    while((db_binding = (krb5_config_binding *)
	   krb5_config_get_next(context, cf, &top_binding, 
				krb5_config_list, 
				"kdc", 
				"database",
				NULL))) {
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
	di = calloc(1, sizeof(*di));
	di->realm = strdup(p);
	p = krb5_config_get_string(context, db_binding, "dbname", NULL);
	if(p)
	    di->dbname = strdup(p);
	p = krb5_config_get_string(context, db_binding, "mkey_file", NULL);
	if(p)
	    di->mkey_file = strdup(p);
	*dt = di;
	dt = &di->next;
    }
    if(default_binding) {
	di = calloc(1, sizeof(*di));
	p = krb5_config_get_string(context, default_binding, "dbname", NULL);
	if(p) {
	    di->dbname = strdup(p);
	    default_dbname = p;
	}
	p = krb5_config_get_string(context, default_binding, "mkey_file", NULL);
	if(p) {
	    di->mkey_file = strdup(p);
	    default_mkey = p;
	}
	*dt = di;
	dt = &di->next;
    } else {
	di = calloc(1, sizeof(*di));
	di->dbname = strdup(default_dbname);
	di->mkey_file = strdup(default_mkey);
	*dt = di;
	dt = &di->next;
    }
    for(di = databases; di; di = di->next) {
	if(di->dbname == NULL)
	    di->dbname = strdup(default_dbname);
	if(di->mkey_file == NULL) {
	    p = strrchr(di->dbname, '.');
	    if(p == NULL || strchr(p, '/') != NULL)
		asprintf(&di->mkey_file, "%s.mkey", di->dbname);
	    else
		asprintf(&di->mkey_file, "%.*s.mkey", 
			 (int)(p - di->dbname), di->dbname);
	}
    }
}

void
configure(int argc, char **argv)
{
    krb5_config_section *cf = NULL;
    int optind = 0;
    int e;
    const char *p;
    
    while((e = getarg(args, num_args, argc, argv, &optind)))
	warnx("error at argument `%s'", argv[optind]);

    if(help_flag)
	usage (0);

    if (version_flag) {
	print_version(NULL);
	exit(0);
    }

    argc -= optind;
    argv += optind;

    if (argc != 0)
	usage(1);
    
    if(config_file == NULL)
	config_file = HDB_DB_DIR "/kdc.conf";
    
    if(krb5_config_parse_file(config_file, &cf))
	cf = NULL;
    
    if(keyfile == NULL){
	p = krb5_config_get_string (context, cf, 
				    "kdc",
				    "key-file",
				    NULL);
	if(p)
	    keyfile = strdup(p);
    }


    get_dbinfo(cf);
    
    if(max_request_str){
	max_request = parse_bytes(max_request_str, NULL);
    }

    if(max_request == 0){
	p = krb5_config_get_string (context,
				    cf, 
				    "kdc",
				    "max-request",
				    NULL);
	if(p)
	    max_request = parse_bytes(p, NULL);
    }
    
    if(require_preauth == -1)
	require_preauth = krb5_config_get_bool(context, cf, "kdc", 
					       "require-preauth", NULL);

    if(port_str == NULL){
	p = krb5_config_get_string(context, cf, "kdc", "ports", NULL);
	if (p != NULL)
	    port_str = strdup(p);
    }
    if(enable_http == -1)
	enable_http = krb5_config_get_bool(context, cf, "kdc", 
					   "enable-http", NULL);
    check_ticket_addresses = 
	krb5_config_get_bool(context, cf, "kdc", 
			     "check-ticket-addresses", NULL);
    allow_null_ticket_addresses = 
	krb5_config_get_bool(context, cf, "kdc", 
			     "allow-null-ticket-addresses", NULL);
#ifdef KRB4
    if(v4_realm == NULL){
	p = krb5_config_get_string (context, cf, 
				    "kdc",
				    "v4-realm",
				    NULL);
	if(p)
	    v4_realm = strdup(p);
    }
#endif
#ifdef KASERVER
    if (enable_kaserver == -1)
	enable_kaserver = krb5_config_get_bool_default(context, cf, TRUE,
						       "kdc",
						       "enable-kaserver",
						       NULL);
#endif

    encode_as_rep_as_tgs_rep = krb5_config_get_bool(context, cf, "kdc", 
						    "encode_as_rep_as_tgs_rep", 
						    NULL);

    kdc_warn_pwexpire = krb5_config_get_time (context, cf,
					      "kdc",
					      "kdc_warn_pwexpire",
					      NULL);
    kdc_openlog(cf);
    if(cf)
	krb5_config_file_free (context, cf);
    if(max_request == 0)
	max_request = 64 * 1024;
    if(require_preauth == -1)
	require_preauth = 1;
    if (port_str == NULL)
	port_str = "+";
#ifdef KRB4
    if(v4_realm == NULL){
	v4_realm = malloc(40); /* REALM_SZ */
	krb_get_lrealm(v4_realm, 1);
    }
#endif
}
