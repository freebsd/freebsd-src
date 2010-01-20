/*
 * Copyright (c) 2004 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: copy_cred_cache.c 15542 2005-07-01 07:20:54Z lha $");
#endif

#include <stdlib.h>
#include <krb5.h>
#include <roken.h>
#include <getarg.h>
#include <parse_units.h>
#include <parse_time.h>

static int krbtgt_only_flag;
static char *service_string;
static char *enctype_string;
static char *flags_string;
static char *valid_string;
static int fcache_version;
static int help_flag;
static int version_flag;

static struct getargs args[] = {
    { "krbtgt-only", 0, arg_flag, &krbtgt_only_flag,
      "only copy local krbtgt" },
    { "service", 0, arg_string, &service_string,
      "limit to this service", "principal" },
    { "enctype", 0, arg_string, &enctype_string,
      "limit to this enctype", "enctype" },
    { "flags", 0, arg_string, &flags_string,
      "limit to these flags", "ticketflags" },
    { "valid-for", 0, arg_string, &valid_string, 
      "limit to creds valid for at least this long", "time" },
    { "fcache-version", 0, arg_integer, &fcache_version,
      "file cache version to create" },
    { "version", 0, arg_flag, &version_flag },
    { "help", 'h', arg_flag, &help_flag }
};

static void
usage(int ret)
{
    arg_printusage(args,
		   sizeof(args) / sizeof(*args),
		   NULL,
		   "[from-cache] to-cache");
    exit(ret);
}

static int32_t
bitswap32(int32_t b)
{
    int32_t r = 0;
    int i;
    for (i = 0; i < 32; i++) {
	r = r << 1 | (b & 1);
	b = b >> 1;
    }
    return r;
}

static void
parse_ticket_flags(krb5_context context,
		   const char *string, krb5_ticket_flags *ret_flags)
{
    TicketFlags ff;
    int flags = parse_flags(string, asn1_TicketFlags_units(), 0);
    if (flags == -1)	/* XXX */
	krb5_errx(context, 1, "bad flags specified: \"%s\"", string);

    memset(&ff, 0, sizeof(ff));
    ff.proxy = 1;
    if (parse_flags("proxy", asn1_TicketFlags_units(), 0) == TicketFlags2int(ff))
	ret_flags->i = flags;
    else
	ret_flags->i = bitswap32(flags);
}

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    int optidx = 0;
    const char *from_name, *to_name;
    krb5_ccache from_ccache, to_ccache;
    krb5_flags whichfields = 0;
    krb5_creds mcreds;
    unsigned int matched;

    setprogname(argv[0]);

    memset(&mcreds, 0, sizeof(mcreds));

    if (getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage(0);

    if (version_flag) {
	print_version(NULL);
	exit(0);
    }
    argc -= optidx;
    argv += optidx;

    if (argc < 1 || argc > 2)
	usage(1);

    if (krb5_init_context(&context))
	errx(1, "krb5_init_context failed");

    if (service_string) {
	ret = krb5_parse_name(context, service_string, &mcreds.server);
	if (ret)
	    krb5_err(context, 1, ret, "%s", service_string);
    }
    if (enctype_string) {
	krb5_enctype enctype;
	ret = krb5_string_to_enctype(context, enctype_string, &enctype);
	if (ret)
	    krb5_err(context, 1, ret, "%s", enctype_string);
	whichfields |= KRB5_TC_MATCH_KEYTYPE;
	mcreds.session.keytype = enctype;
    }
    if (flags_string) {
	parse_ticket_flags(context, flags_string, &mcreds.flags);
	whichfields |= KRB5_TC_MATCH_FLAGS;
    }
    if (valid_string) {
	time_t t = parse_time(valid_string, "s");
	if(t < 0)
	    errx(1, "unknown time \"%s\"", valid_string);
	mcreds.times.endtime = time(NULL) + t;
	whichfields |= KRB5_TC_MATCH_TIMES;
    }
    if (fcache_version)
	krb5_set_fcache_version(context, fcache_version);

    if (argc == 1) {
	from_name = krb5_cc_default_name(context);
	to_name = argv[0];
    } else {
	from_name = argv[0];
	to_name = argv[1];
    }

    ret = krb5_cc_resolve(context, from_name, &from_ccache);
    if (ret)
	krb5_err(context, 1, ret, "%s", from_name);

    if (krbtgt_only_flag) {
	krb5_principal client;
	ret = krb5_cc_get_principal(context, from_ccache, &client);
	if (ret)
	    krb5_err(context, 1, ret, "getting default principal");
	ret = krb5_make_principal(context, &mcreds.server,
				  krb5_principal_get_realm(context, client),
				  KRB5_TGS_NAME,
				  krb5_principal_get_realm(context, client),
				  NULL);
	if (ret)
	    krb5_err(context, 1, ret, "constructing krbtgt principal");
	krb5_free_principal(context, client);
    }
    ret = krb5_cc_resolve(context, to_name, &to_ccache);
    if (ret)
	krb5_err(context, 1, ret, "%s", to_name);

    ret = krb5_cc_copy_cache_match(context, from_ccache, to_ccache,
				   whichfields, &mcreds, &matched);
    if (ret)
	krb5_err(context, 1, ret, "copying cred cache");

    krb5_cc_close(context, from_ccache);
    if(matched == 0)
	krb5_cc_destroy(context, to_ccache);
    else
	krb5_cc_close(context, to_ccache);
    krb5_free_context(context);
    return matched == 0;
}
