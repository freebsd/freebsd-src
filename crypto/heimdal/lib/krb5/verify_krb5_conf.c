/*
 * Copyright (c) 1999 - 2001 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"
#include <getarg.h>
#include <parse_bytes.h>
#include <err.h>
RCSID("$Id: verify_krb5_conf.c,v 1.7 2001/09/03 05:42:35 assar Exp $");

/* verify krb5.conf */

static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"version",	0,	arg_flag,	&version_flag,
     "print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,
     NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args)/sizeof(*args),
		    NULL,
		    "[config-file]");
    exit (ret);
}

static int
check_bytes(krb5_context context, const char *path, char *data)
{
    if(parse_bytes(data, NULL) == -1) {
	krb5_warnx(context, "%s: failed to parse \"%s\" as size", path, data);
	return 1;
    }
    return 0;
}

static int
check_time(krb5_context context, const char *path, char *data)
{
    if(parse_time(data, NULL) == -1) {
	krb5_warnx(context, "%s: failed to parse \"%s\" as time", path, data);
	return 1;
    }
    return 0;
}

static int
check_numeric(krb5_context context, const char *path, char *data)
{
    long int v;
    char *end;
    v = strtol(data, &end, 0);
    if(*end != '\0') {
	krb5_warnx(context, "%s: failed to parse \"%s\" as a number", 
		   path, data);
	return 1;
    }
    return 0;
}

static int
check_boolean(krb5_context context, const char *path, char *data)
{
    long int v;
    char *end;
    if(strcasecmp(data, "yes") == 0 ||
       strcasecmp(data, "true") == 0 ||
       strcasecmp(data, "no") == 0 ||
       strcasecmp(data, "false") == 0)
	return 0;
    v = strtol(data, &end, 0);
    if(*end != '\0') {
	krb5_warnx(context, "%s: failed to parse \"%s\" as a boolean", 
		   path, data);
	return 1;
    }
    return 0;
}

static int
check_host(krb5_context context, const char *path, char *data)
{
    int ret;
    char hostname[128];
    const char *p = data;
    struct addrinfo *ai;
    /* XXX data could be a list of hosts that this code can't handle */
    /* XXX copied from krbhst.c */
    if(strncmp(p, "http://", 7) == 0){
        p += 7;
    } else if(strncmp(p, "http/", 5) == 0) {
        p += 5;
    }else if(strncmp(p, "tcp/", 4) == 0){
        p += 4;
    } else if(strncmp(p, "udp/", 4) == 0) {
        p += 4;
    }
    if(strsep_copy(&p, ":", hostname, sizeof(hostname)) < 0) {
	return 1;
    }
    hostname[strcspn(hostname, "/")] = '\0';
    ret = getaddrinfo(hostname, "telnet" /* XXX */, NULL, &ai);
    if(ret != 0) {
	if(ret == EAI_NODATA)
	    krb5_warnx(context, "%s: host not found (%s)", path, hostname);
	else
	    krb5_warnx(context, "%s: %s (%s)", path, gai_strerror(ret), hostname);
	return 1;
    }
    return 0;
}

typedef int (*check_func_t)(krb5_context, const char*, char*);
struct entry {
    const char *name;
    int type;
    void *check_data;
};

struct entry all_strings[] = {
    { "", krb5_config_string, NULL },
    { NULL }
};

struct entry v4_name_convert_entries[] = {
    { "host", krb5_config_list, all_strings },
    { "plain", krb5_config_list, all_strings },
    { NULL }
};

struct entry libdefaults_entries[] = {
    { "accept_null_addresses", krb5_config_string, check_boolean },
    { "capath", krb5_config_list, all_strings },
    { "clockskew", krb5_config_string, check_time },
    { "date_format", krb5_config_string, NULL },
    { "default_etypes", krb5_config_string, NULL },
    { "default_etypes_des", krb5_config_string, NULL },
    { "default_keytab_modify_name", krb5_config_string, NULL },
    { "default_keytab_name", krb5_config_string, NULL },
    { "default_realm", krb5_config_string, NULL },
    { "dns_proxy", krb5_config_string, NULL },
    { "egd_socket", krb5_config_string, NULL },
    { "encrypt", krb5_config_string, check_boolean },
    { "extra_addresses", krb5_config_string, NULL },
    { "fcache_version", krb5_config_string, check_numeric },
    { "forward", krb5_config_string, check_boolean },
    { "forwardable", krb5_config_string, check_boolean },
    { "http_proxy", krb5_config_string, check_host /* XXX */ },
    { "ignore_addresses", krb5_config_string, NULL },
    { "kdc_timeout", krb5_config_string, check_time },
    { "kdc_timesync", krb5_config_string, check_boolean },
    { "krb4_get_tickets", krb5_config_string, check_boolean },
    { "log_utc", krb5_config_string, check_boolean },
    { "maxretries", krb5_config_string, check_numeric },
    { "scan_interfaces", krb5_config_string, check_boolean },
    { "srv_lookup", krb5_config_string, check_boolean },
    { "srv_try_txt", krb5_config_string, check_boolean }, 
    { "ticket_lifetime", krb5_config_string, check_time },
    { "time_format", krb5_config_string, NULL },
    { "transited_realms_reject", krb5_config_string, NULL },
    { "v4_instance_resolve", krb5_config_string, check_boolean },
    { "v4_name_convert", krb5_config_list, v4_name_convert_entries },
    { "verify_ap_req_nofail", krb5_config_string, check_boolean },
    { NULL }
};

struct entry appdefaults_entries[] = {
    { "forwardable", krb5_config_string, check_boolean },
    { "proxiable", krb5_config_string, check_boolean },
    { "ticket_lifetime", krb5_config_string, check_time },
    { "renew_lifetime", krb5_config_string, check_time },
    { "no-addresses", krb5_config_string, check_boolean },
#if 0
    { "anonymous", krb5_config_string, check_boolean },
#endif
    { "", krb5_config_list, appdefaults_entries },
    { NULL }
};

struct entry realms_entries[] = {
    { "forwardable", krb5_config_string, check_boolean },
    { "proxiable", krb5_config_string, check_boolean },
    { "ticket_lifetime", krb5_config_string, check_time },
    { "renew_lifetime", krb5_config_string, check_time },
    { "warn_pwexpire", krb5_config_string, check_time },
    { "kdc", krb5_config_string, check_host },
    { "admin_server", krb5_config_string, check_host },
    { "kpasswd_server", krb5_config_string, check_host },
    { "krb524_server", krb5_config_string, check_host },
    { "v4_name_convert", krb5_config_list, v4_name_convert_entries },
    { "v4_instance_convert", krb5_config_list, all_strings },
    { "v4_domains", krb5_config_string, NULL },
    { "default_domain", krb5_config_string, NULL },
    { NULL }
};

struct entry realms_foobar[] = {
    { "", krb5_config_list, realms_entries },
    { NULL }
};


struct entry kdc_database_entries[] = {
    { "realm", krb5_config_string, NULL },
    { "dbname", krb5_config_string, NULL },
    { "mkey_file", krb5_config_string, NULL },
    { NULL }
};

struct entry kdc_entries[] = {
    { "database", krb5_config_list, kdc_database_entries },
    { "key-file", krb5_config_string, NULL },
    { "logging", krb5_config_string, NULL },
    { "max-request", krb5_config_string, check_bytes },
    { "require-preauth", krb5_config_string, check_boolean },
    { "ports", krb5_config_string, NULL },
    { "addresses", krb5_config_string, NULL },
    { "enable-kerberos4", krb5_config_string, check_boolean },
    { "enable-524", krb5_config_string, check_boolean },
    { "enable-http", krb5_config_string, check_boolean },
    { "check_ticket-addresses", krb5_config_string, check_boolean },
    { "allow-null-addresses", krb5_config_string, check_boolean },
    { "allow-anonymous", krb5_config_string, check_boolean },
    { "v4_realm", krb5_config_string, NULL },
    { "enable-kaserver", krb5_config_string, check_boolean },
    { "encode_as_rep_as_tgs_rep", krb5_config_string, check_boolean },
    { "kdc_warn_pwexpire", krb5_config_string, check_time },
    { NULL }
};

struct entry kadmin_entries[] = {
    { "password_lifetime", krb5_config_string, check_time },
    { "default_keys", krb5_config_string, NULL },
    { "use_v4_salt", krb5_config_string, NULL },
    { NULL }
};
struct entry toplevel_sections[] = {
    { "libdefaults" , krb5_config_list, libdefaults_entries },
    { "realms", krb5_config_list, realms_foobar },
    { "domain_realm", krb5_config_list, all_strings },
    { "logging", krb5_config_list, all_strings },
    { "kdc", krb5_config_list, kdc_entries },
    { "kadmin", krb5_config_list, kadmin_entries },
    { "appdefaults", krb5_config_list, appdefaults_entries },
    { NULL }
};


static int
check_section(krb5_context context, const char *path, krb5_config_section *cf, 
	      struct entry *entries)
{
    int error = 0;
    krb5_config_section *p;
    struct entry *e;
    
    char *local;
    
    for(p = cf; p != NULL; p = p->next) {
	asprintf(&local, "%s/%s", path, p->name);
	for(e = entries; e->name != NULL; e++) {
	    if(*e->name == '\0' || strcmp(e->name, p->name) == 0) {
		if(e->type != p->type) {
		    krb5_warnx(context, "%s: unknown or wrong type", local);
		    error |= 1;
		} else if(p->type == krb5_config_string && e->check_data != NULL) {
		    error |= (*(check_func_t)e->check_data)(context, local, p->u.string);
		} else if(p->type == krb5_config_list && e->check_data != NULL) {
		    error |= check_section(context, local, p->u.list, e->check_data);
		}
		break;
	    }
	}
	if(e->name == NULL) {
	    krb5_warnx(context, "%s: unknown entry", local);
	    error |= 1;
	}
	free(local);
    }
    return error;
}


int
main(int argc, char **argv)
{
    krb5_context context;
    const char *config_file = NULL;
    krb5_error_code ret;
    krb5_config_section *tmp_cf;
    int optind = 0;

    setprogname (argv[0]);

    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed");

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optind))
	usage(1);
    
    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    argc -= optind;
    argv += optind;

    if (argc == 0) {
	config_file = getenv("KRB5_CONFIG");
	if (config_file == NULL)
	    config_file = krb5_config_file;
    } else if (argc == 1) {
	config_file = argv[0];
    } else {
	usage (1);
    }
    
    ret = krb5_config_parse_file (context, config_file, &tmp_cf);
    if (ret != 0) {
	krb5_warn (context, ret, "krb5_config_parse_file");
	return 1;
    }

    return check_section(context, "", tmp_cf, toplevel_sections);
}
