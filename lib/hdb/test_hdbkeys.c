/*
 * Copyright (c) 2005 Kungliga Tekniska Högskolan
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

#include "hdb_locl.h"
#include <getarg.h>
#include <base64.h>

static int help_flag;
static int version_flag;
static int kvno_integer = 1;

struct getargs args[] = {
    { "kvno",		'd',	arg_integer, &kvno_integer },
    { "help",		'h',	arg_flag,   &help_flag },
    { "version",	0,	arg_flag,   &version_flag }
};

static int num_args = sizeof(args) / sizeof(args[0]);

int
main(int argc, char **argv)
{
    krb5_principal principal;
    krb5_context context;
    char *principal_str, *password_str, *str;
    int ret, o = 0;
    hdb_keyset keyset;
    size_t length, len;
    void *data;

    setprogname(argv[0]);

    if(getarg(args, num_args, argc, argv, &o))
	krb5_std_usage(1, args, num_args);

    if(help_flag)
	krb5_std_usage(0, args, num_args);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    if (argc != 3)
	errx(1, "username and password missing");

    principal_str = argv[1];
    password_str = argv[2];

    ret = krb5_parse_name (context, principal_str, &principal);
    if (ret)
	krb5_err (context, 1, ret, "krb5_parse_name %s", principal_str);

    memset(&keyset, 0, sizeof(keyset));

    keyset.kvno = kvno_integer;

    ret = hdb_generate_key_set_password(context, principal, password_str,
					&keyset.keys.val, &len);
    if (ret)
	krb5_err(context, 1, ret, "hdb_generate_key_set_password");
    keyset.keys.len = len;

    if (keyset.keys.len == 0)
	krb5_errx (context, 1, "hdb_generate_key_set_password length 0");

    krb5_free_principal (context, principal);

    ASN1_MALLOC_ENCODE(hdb_keyset, data, length, &keyset, &len, ret);
    if (ret)
	krb5_errx(context, 1, "encode keyset");
    if (len != length)
	krb5_abortx(context, "foo");

    krb5_free_context(context);

    ret = base64_encode(data, length, &str);
    if (ret < 0)
	errx(1, "base64_encode");

    printf("keyset: %s\n", str);

    free(data);

    return 0;
}
