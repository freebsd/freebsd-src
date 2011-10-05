/*
 * Copyright (c) 2009 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "krb5_locl.h"
#include <err.h>
#include <getarg.h>

static char *password_str;

static krb5_error_code
lr_proc(krb5_context context, krb5_last_req_entry **e, void *ctx)
{
    while (e && *e) {
	printf("e type: %d value: %d\n", (*e)->lr_type, (int)(*e)->value);
	e++;
    }
    return 0;
}

static void
test_get_init_creds(krb5_context context,
		    krb5_principal client)
{
    krb5_error_code ret;
    krb5_get_init_creds_opt *opt;
    krb5_creds cred;

    ret = krb5_get_init_creds_opt_alloc(context, &opt);
    if (ret)
	krb5_err(context, 1, ret, "krb5_get_init_creds_opt_alloc");


    ret = krb5_get_init_creds_opt_set_process_last_req(context,
						       opt,
						       lr_proc,
						       NULL);
    if (ret)
	krb5_err(context, 1, ret,
		 "krb5_get_init_creds_opt_set_process_last_req");

    ret = krb5_get_init_creds_password(context,
				       &cred,
				       client,
				       password_str,
				       krb5_prompter_posix,
				       NULL,
				       0,
				       NULL,
				       opt);
    if (ret)
	krb5_err(context, 1, ret, "krb5_get_init_creds_password");

    krb5_get_init_creds_opt_free(context, opt);
}

static char *client_str = NULL;
static int debug_flag	= 0;
static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"client",	0,	arg_string,	&client_str,
     "client principal to use", NULL },
    {"password",0,	arg_string,	&password_str,
     "password", NULL },
    {"debug",	'd',	arg_flag,	&debug_flag,
     "turn on debuggin", NULL },
    {"version",	0,	arg_flag,	&version_flag,
     "print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,
     NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args, sizeof(args)/sizeof(*args), NULL, "hostname ...");
    exit (ret);
}


int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;
    int optidx = 0, errors = 0;
    krb5_principal client;

    setprogname(argv[0]);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    if(client_str == NULL)
	errx(1, "client is not set");

    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    ret = krb5_parse_name(context, client_str, &client);
    if (ret)
	krb5_err(context, 1, ret, "krb5_parse_name: %d", ret);

    test_get_init_creds(context, client);

    krb5_free_context(context);

    return errors;
}
