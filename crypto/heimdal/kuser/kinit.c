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

#include "kuser_locl.h"
RCSID("$Id: kinit.c,v 1.60 2000/02/01 14:06:33 joda Exp $");

int forwardable		= 0;
int proxiable		= 0;
int renewable		= 0;
int renew_flag		= 0;
int validate_flag	= 0;
int version_flag	= 0;
int help_flag		= 0;
int addrs_flag		= 1;
char *lifetime 		= NULL;
char *renew_life	= NULL;
char *server		= NULL;
char *cred_cache	= NULL;
char *start_str		= NULL;
struct getarg_strings etype_str;
int use_keytab		= 0;
char *keytab_str	= NULL;
#ifdef KRB4
extern int do_afslog;
extern int get_v4_tgt;
#endif
int fcache_version;

struct getargs args[] = {
#ifdef KRB4
    { "524init", 	'4', arg_flag, &get_v4_tgt,
      "obtain version 4 TGT" },
    
    { "afslog", 	0  , arg_flag, &do_afslog,
      "obtain afs tokens"  },
#endif
    { "cache", 		'c', arg_string, &cred_cache,
      "credentials cache", "cachename" },

    { "forwardable",	'f', arg_flag, &forwardable,
      "get forwardable tickets"},

    { "keytab",         't', arg_string, &keytab_str,
      "keytab to use", "keytabname" },

    { "lifetime",	'l', arg_string, &lifetime,
      "lifetime of tickets", "seconds"},

    { "proxiable",	'p', arg_flag, &proxiable,
      "get proxiable tickets" },

    { "renew",          'R', arg_flag, &renew_flag,
      "renew TGT" },

    { "renewable",	0,   arg_flag, &renewable,
      "get renewable tickets" },

    { "renewable-life",	'r', arg_string, &renew_life,
      "renewable lifetime of tickets", "seconds" },

    { "server", 	'S', arg_string, &server,
      "server to get ticket for", "principal" },

    { "start-time",	's', arg_string, &start_str,
      "when ticket gets valid", "seconds" },

    { "use-keytab",     'k', arg_flag, &use_keytab,
      "get key from keytab" },

    { "validate",	'v', arg_flag, &validate_flag,
      "validate TGT" },

    { "enctypes",	'e', arg_strings, &etype_str,
      "encryption type to use", "enctype" },

    { "fcache-version", 0,   arg_integer, &fcache_version,
      "file cache version to create" },

    { "addresses",	0,   arg_negative_flag,	&addrs_flag,
      "request a ticket with no addresses" },

    { "version", 	0,   arg_flag, &version_flag },
    { "help",		0,   arg_flag, &help_flag }
};

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args)/sizeof(*args),
		    NULL,
		    "[principal]");
    exit (ret);
}

static int
renew_validate(krb5_context context, 
	       int renew,
	       int validate,
	       krb5_ccache cache, 
	       const char *server,
	       krb5_deltat life)
{
    krb5_error_code ret;
    krb5_creds in, *out;
    krb5_kdc_flags flags;

    memset(&in, 0, sizeof(in));

    ret = krb5_cc_get_principal(context, cache, &in.client);
    if(ret) {
	krb5_warn(context, ret, "krb5_cc_get_principal");
	return ret;
    }
    if(server) {
	ret = krb5_parse_name(context, server, &in.server);
	if(ret) {
	    krb5_warn(context, ret, "krb5_parse_name");
	    goto out;
	}
    } else {
	krb5_realm *client_realm = krb5_princ_realm (context, in.client);

	ret = krb5_make_principal(context, &in.server, *client_realm,
				  KRB5_TGS_NAME, *client_realm, NULL);
	if(ret) {
	    krb5_warn(context, ret, "krb5_make_principal");
	    goto out;
	}
    }
    flags.i = 0;
    flags.b.renewable   = flags.b.renew = renew;
    flags.b.validate    = validate;
    flags.b.forwardable = forwardable;
    flags.b.proxiable   = proxiable;
    if(life)
	in.times.endtime = time(NULL) + life;

    ret = krb5_get_kdc_cred(context,
			    cache,
			    flags,
			    NULL,
			    NULL,
			    &in,
			    &out);
    if(ret) {
	krb5_warn(context, ret, "krb5_get_kdc_cred");
	goto out;
    }
    ret = krb5_cc_initialize(context, cache, in.client);
    if(ret) {
	krb5_free_creds (context, out);
	krb5_warn(context, ret, "krb5_cc_initialize");
	goto out;
    }
    ret = krb5_cc_store_cred(context, cache, out);
    krb5_free_creds (context, out);
    if(ret) {
	krb5_warn(context, ret, "krb5_cc_store_cred");
	goto out;
    }
out:
    krb5_free_creds_contents(context, &in);
    return ret;
}

int
main (int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_ccache  ccache;
    krb5_principal principal;
    krb5_creds cred;
    int optind = 0;
    krb5_get_init_creds_opt opt;
    krb5_deltat start_time = 0;
    krb5_deltat ticket_life = 0;
    krb5_addresses no_addrs;

    set_progname (argv[0]);
    memset(&cred, 0, sizeof(cred));
    
    ret = krb5_init_context (&context);
    if (ret)
	errx(1, "krb5_init_context failed: %u", ret);
  
    forwardable = krb5_config_get_bool (context, NULL,
					"libdefaults",
					"forwardable",
					NULL);

#ifdef KRB4
    get_v4_tgt = krb5_config_get_bool_default (context, NULL,
					       get_v4_tgt,
					       "libdefaults",
					       "krb4_get_tickets",
					       NULL);
#endif

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optind))
	usage(1);
    
    if (help_flag)
	usage (0);

    if(version_flag) {
	print_version(NULL);
	exit(0);
    }

    if(fcache_version)
	krb5_set_fcache_version(context, fcache_version);

    if(cred_cache) 
	ret = krb5_cc_resolve(context, cred_cache, &ccache);
    else
	ret = krb5_cc_default (context, &ccache);
    if (ret)
	krb5_err (context, 1, ret, "resolving credentials cache");

    if (lifetime) {
	int tmp = parse_time (lifetime, "s");
	if (tmp < 0)
	    errx (1, "unparsable time: %s", lifetime);

	ticket_life = tmp;
    }
    if(renew_flag || validate_flag) {
	ret = renew_validate(context, renew_flag, validate_flag, 
			     ccache, server, ticket_life);
	exit(ret != 0);
    }

    krb5_get_init_creds_opt_init (&opt);
    
    krb5_get_init_creds_opt_set_forwardable (&opt, forwardable);
    krb5_get_init_creds_opt_set_proxiable (&opt, proxiable);

    if (!addrs_flag) {
	no_addrs.len = 0;
	no_addrs.val = NULL;

	krb5_get_init_creds_opt_set_address_list (&opt, &no_addrs);
    }

    if(renew_life) {
	int tmp = parse_time (renew_life, "s");
	if (tmp < 0)
	    errx (1, "unparsable time: %s", renew_life);

	krb5_get_init_creds_opt_set_renew_life (&opt, tmp);
    } else if (renewable)
	krb5_get_init_creds_opt_set_renew_life (&opt, 1 << 30);

    if(ticket_life != 0)
	krb5_get_init_creds_opt_set_tkt_life (&opt, ticket_life);

    if(start_str) {
	int tmp = parse_time (start_str, "s");
	if (tmp < 0)
	    errx (1, "unparsable time: %s", start_str);

	start_time = tmp;
    }

    if(etype_str.num_strings) {
	krb5_enctype *enctype = NULL;
	int i;
	enctype = malloc(etype_str.num_strings * sizeof(*enctype));
	if(enctype == NULL)
	    errx(1, "out of memory");
	for(i = 0; i < etype_str.num_strings; i++) {
	    ret = krb5_string_to_enctype(context, 
					 etype_str.strings[i], 
					 &enctype[i]);
	    if(ret)
		errx(1, "unrecognized enctype: %s", etype_str.strings[i]);
	}
	krb5_get_init_creds_opt_set_etype_list(&opt, enctype, 
					       etype_str.num_strings);
    }

    argc -= optind;
    argv += optind;

    if (argc > 1)
	usage (1);

    if (argv[0]) {
	ret = krb5_parse_name (context, argv[0], &principal);
	if (ret)
	    krb5_err (context, 1, ret, "krb5_parse_name");
    } else
	principal = NULL;

    if(use_keytab || keytab_str) {
	krb5_keytab kt;
	if(keytab_str)
	    ret = krb5_kt_resolve(context, keytab_str, &kt);
	else
	    ret = krb5_kt_default(context, &kt);
	if (ret)
	    krb5_err (context, 1, ret, "resolving keytab");
	ret = krb5_get_init_creds_keytab (context,
					  &cred,
					  principal,
					  kt,
					  start_time,
					  server,
					  &opt);
	krb5_kt_close(context, kt);
    } else 
	ret = krb5_get_init_creds_password (context,
					    &cred,
					    principal,
					    NULL,
					    krb5_prompter_posix,
					    NULL,
					    start_time,
					    server,
					    &opt);
    switch(ret){
    case 0:
	break;
    case KRB5_LIBOS_PWDINTR: /* don't print anything if it was just C-c:ed */
	exit(1);
    case KRB5KRB_AP_ERR_BAD_INTEGRITY:
    case KRB5KRB_AP_ERR_MODIFIED:
	krb5_errx(context, 1, "Password incorrect");
	break;
    default:
	krb5_err(context, 1, ret, "krb5_get_init_creds");
    }

    ret = krb5_cc_initialize (context, ccache, cred.client);
    if (ret)
	krb5_err (context, 1, ret, "krb5_cc_initialize");
    
    ret = krb5_cc_store_cred (context, ccache, &cred);
    if (ret)
	krb5_err (context, 1, ret, "krb5_cc_store_cred");

#ifdef KRB4
    if(get_v4_tgt) {
	CREDENTIALS c;
	ret = krb524_convert_creds_kdc(context, ccache, &cred, &c);
	if(ret)
	    krb5_warn(context, ret, "converting creds");
	else
	    tf_setup(&c, c.pname, c.pinst);
	memset(&c, 0, sizeof(c));
    }
    if(do_afslog && k_hasafs())
	krb5_afslog(context, ccache, NULL, NULL);
#endif
    krb5_free_creds_contents (context, &cred);
    krb5_cc_close (context, ccache);
    krb5_free_context (context);
    return 0;
}
