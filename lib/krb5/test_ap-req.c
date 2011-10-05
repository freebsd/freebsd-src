/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
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

#include <config.h>

#include <sys/types.h>
#include <stdio.h>
#include <krb5.h>
#include <err.h>
#include <getarg.h>
#include <roken.h>

static int verify_pac = 0;
static int server_any = 0;
static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"verify-pac",0,	arg_flag,	&verify_pac,
     "verify the PAC", NULL },
    {"server-any",0,	arg_flag,	&server_any,
     "let server pick the principal", NULL },
    {"version",	0,	arg_flag,	&version_flag,
     "print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,
     NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args, sizeof(args)/sizeof(*args), NULL, "...");
    exit (ret);
}


static void
test_ap(krb5_context context,
	krb5_principal target,
	krb5_principal server,
	krb5_keytab keytab,
	krb5_ccache ccache,
	const krb5_flags client_flags)
{
    krb5_error_code ret;
    krb5_auth_context client_ac = NULL, server_ac = NULL;
    krb5_data data;
    krb5_flags server_flags;
    krb5_ticket *ticket = NULL;
    int32_t server_seq, client_seq;

    ret = krb5_mk_req_exact(context,
			    &client_ac,
			    client_flags,
			    target,
			    NULL,
			    ccache,
			    &data);
    if (ret)
	krb5_err(context, 1, ret, "krb5_mk_req_exact");

    ret = krb5_rd_req(context,
		      &server_ac,
		      &data,
		      server,
		      keytab,
		      &server_flags,
		      &ticket);
    if (ret)
	krb5_err(context, 1, ret, "krb5_rd_req");


    if (server_flags & AP_OPTS_MUTUAL_REQUIRED) {
	krb5_ap_rep_enc_part *repl;

	krb5_data_free(&data);

	if ((client_flags & AP_OPTS_MUTUAL_REQUIRED) == 0)
	    krb5_errx(context, 1, "client flag missing mutual req");

	ret = krb5_mk_rep (context, server_ac, &data);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_mk_rep");

	ret = krb5_rd_rep (context,
			   client_ac,
			   &data,
			   &repl);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_rd_rep");

	krb5_free_ap_rep_enc_part (context, repl);
    } else {
	if (client_flags & AP_OPTS_MUTUAL_REQUIRED)
	    krb5_errx(context, 1, "server flag missing mutual req");
    }

    krb5_auth_con_getremoteseqnumber(context, server_ac, &server_seq);
    krb5_auth_con_getremoteseqnumber(context, client_ac, &client_seq);
    if (server_seq != client_seq)
	krb5_errx(context, 1, "seq num differ");

    krb5_auth_con_getlocalseqnumber(context, server_ac, &server_seq);
    krb5_auth_con_getlocalseqnumber(context, client_ac, &client_seq);
    if (server_seq != client_seq)
	krb5_errx(context, 1, "seq num differ");

    krb5_data_free(&data);
    krb5_auth_con_free(context, client_ac);
    krb5_auth_con_free(context, server_ac);

    if (verify_pac) {
	krb5_pac pac;

	ret = krb5_ticket_get_authorization_data_type(context,
						      ticket,
						      KRB5_AUTHDATA_WIN2K_PAC,
						      &data);
	if (ret)
	    krb5_err(context, 1, ret, "get pac");

	ret = krb5_pac_parse(context, data.data, data.length, &pac);
	if (ret)
	    krb5_err(context, 1, ret, "pac parse");

	krb5_pac_free(context, pac);
    }

    krb5_free_ticket(context, ticket);
}


int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;
    int optidx = 0;
    const char *principal, *keytab, *ccache;
    krb5_ccache id;
    krb5_keytab kt;
    krb5_principal sprincipal, server;

    setprogname(argv[0]);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    argc -= optidx;
    argv += optidx;

    if (argc < 3)
	usage(1);

    principal = argv[0];
    keytab = argv[1];
    ccache = argv[2];

    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    ret = krb5_cc_resolve(context, ccache, &id);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_resolve");

    ret = krb5_parse_name(context, principal, &sprincipal);
    if (ret)
	krb5_err(context, 1, ret, "krb5_parse_name");

    ret = krb5_kt_resolve(context, keytab, &kt);
    if (ret)
	krb5_err(context, 1, ret, "krb5_kt_resolve");

    if (server_any)
	server = NULL;
    else
	server = sprincipal;

    test_ap(context, sprincipal, server, kt, id, 0);
    test_ap(context, sprincipal, server, kt, id, AP_OPTS_MUTUAL_REQUIRED);

    krb5_cc_close(context, id);
    krb5_kt_close(context, kt);
    krb5_free_principal(context, sprincipal);

    krb5_free_context(context);

    return ret;
}
