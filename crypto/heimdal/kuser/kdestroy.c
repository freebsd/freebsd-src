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

#include "kuser_locl.h"
RCSID("$Id: kdestroy.c,v 1.12 2000/12/31 07:51:09 assar Exp $");

static const char *cache;
static int help_flag;
static int version_flag;
static int unlog_flag = 1;
static int dest_tkt_flag = 1;

struct getargs args[] = {
    { "cache",		'c', arg_string, &cache, "cache to destroy", "cache" },
    { "unlog",		0,   arg_negative_flag, &unlog_flag,
      "do not destroy tokens", NULL },
    { "delete-v4",	0,   arg_negative_flag, &dest_tkt_flag,
      "do not destroy v4 tickets", NULL },
    { "version", 	0,   arg_flag, &version_flag, NULL, NULL },
    { "help",		'h', arg_flag, &help_flag, NULL, NULL}
};

int num_args = sizeof(args) / sizeof(args[0]);

static void
usage (int status)
{
    arg_printusage (args, num_args, NULL, "");
    exit (status);
}

int
main (int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_ccache  ccache;
    int optind = 0;
    int exit_val = 0;

    set_progname (argv[0]);

    if(getarg(args, num_args, argc, argv, &optind))
	usage(1);
  
    if (help_flag)
	usage (0);
  
    if(version_flag){
	print_version(NULL);
	exit(0);
    }
  
    argc -= optind;
    argv += optind;

    if (argc != 0)
	usage (1);

    ret = krb5_init_context (&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);
  
    if(cache == NULL)
	cache = krb5_cc_default_name(context);

    ret =  krb5_cc_resolve(context, 
			   cache, 
			   &ccache);

    if (ret == 0) {
	ret = krb5_cc_destroy (context, ccache);
	if (ret) {
	    warnx ("krb5_cc_destroy: %s", krb5_get_err_text(context, ret));
	    exit_val = 1;
	}
    } else {
	warnx ("krb5_cc_resolve(%s): %s", cache,
	       krb5_get_err_text(context, ret));
	exit_val = 1;
    }

    krb5_free_context (context);

#if KRB4
    if(dest_tkt_flag && dest_tkt ())
	exit_val = 1;
    if (unlog_flag && k_hasafs ()) {
	if (k_unlog ())
	    exit_val = 1;
    }
#endif

    return exit_val;
}
