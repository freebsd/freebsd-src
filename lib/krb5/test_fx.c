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

struct {
    char *p1;
    char *pepper1;
    krb5_enctype e1;
    char *p2;
    char *pepper2;
    krb5_enctype e2;
    krb5_enctype e3;
    char *key;
    size_t len;
} cf2[] = {
    {
	"key1", "a", ETYPE_AES128_CTS_HMAC_SHA1_96,
	"key2", "b", ETYPE_AES128_CTS_HMAC_SHA1_96,
	ETYPE_AES128_CTS_HMAC_SHA1_96,
	"\x97\xdf\x97\xe4\xb7\x98\xb2\x9e\xb3\x1e\xd7\x28\x02\x87\xa9\x2a",
	16
    },
    {
	"key1", "a", ETYPE_AES256_CTS_HMAC_SHA1_96,
	"key2", "b", ETYPE_AES256_CTS_HMAC_SHA1_96,
	ETYPE_AES256_CTS_HMAC_SHA1_96,
	"\x4d\x6c\xa4\xe6\x29\x78\x5c\x1f\x01\xba\xf5\x5e\x2e\x54\x85\x66"
	"\xb9\x61\x7a\xe3\xa9\x68\x68\xc3\x37\xcb\x93\xb5\xe7\x2b\x1c\x7b",
	32
    },
    {
	"key1", "a", ETYPE_AES128_CTS_HMAC_SHA1_96,
	"key2", "b", ETYPE_AES128_CTS_HMAC_SHA1_96,
	ETYPE_AES256_CTS_HMAC_SHA1_96,
	"\x97\xdf\x97\xe4\xb7\x98\xb2\x9e\xb3\x1e\xd7\x28\x2\x87\xa9\x2a"
	"\x1\x96\xfa\xf2\x44\xf8\x11\x20\xc2\x1c\x51\x17\xb3\xe6\xeb\x98",
	32
    },
    {
	"key1", "a", ETYPE_AES256_CTS_HMAC_SHA1_96,
	"key2", "b", ETYPE_AES256_CTS_HMAC_SHA1_96,
	ETYPE_AES128_CTS_HMAC_SHA1_96,
	"\x4d\x6c\xa4\xe6\x29\x78\x5c\x1f\x01\xba\xf5\x5e\x2e\x54\x85\x66",
	16
    },
    {
	"key1", "a", ETYPE_AES128_CTS_HMAC_SHA1_96,
	"key2", "b", ETYPE_AES256_CTS_HMAC_SHA1_96,
	ETYPE_AES256_CTS_HMAC_SHA1_96,
	"\x88\xbd\xb2\xa9\xf\x3e\x52\x5a\xb0\x5f\x68\xc5\x43\x9a\x4d\x5e"
	"\x9c\x2b\xfd\x2b\x02\x24\xde\x39\xb5\x82\xf4\xbb\x05\xfe\x2\x2e",
	32
    }
};


static void
test_cf2(krb5_context context)
{
    krb5_error_code ret;
    krb5_data pw, p1, p2;
    krb5_salt salt;
    krb5_keyblock k1, k2, k3;
    krb5_crypto c1, c2;
    unsigned int i;

    for (i = 0; i < sizeof(cf2)/sizeof(cf2[0]); i++) {
	pw.data = cf2[i].p1;
	pw.length = strlen(cf2[i].p1);
	salt.salttype = (krb5_salttype)KRB5_PADATA_PW_SALT;
	salt.saltvalue.data = cf2[i].p1;
	salt.saltvalue.length = strlen(cf2[i].p1);

	ret = krb5_string_to_key_data_salt(context,
					   cf2[i].e1,
					   pw,
					   salt,
					   &k1);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_string_to_key_data_salt");

	ret = krb5_crypto_init(context, &k1, 0, &c1);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_crypto_init");

	pw.data = cf2[i].p2;
	pw.length = strlen(cf2[i].p2);
	salt.saltvalue.data = cf2[i].p2;
	salt.saltvalue.length = strlen(cf2[i].p2);

	ret = krb5_string_to_key_data_salt(context,
					   cf2[i].e2,
					   pw,
					   salt,
					   &k2);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_string_to_key_data_salt");

	ret = krb5_crypto_init(context, &k2, 0, &c2);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_crypto_init");


	p1.data = cf2[i].pepper1;
	p1.length = strlen(cf2[i].pepper1);

	p2.data = cf2[i].pepper2;
	p2.length = strlen(cf2[i].pepper2);

	ret = krb5_crypto_fx_cf2(context, c1, c2, &p1, &p2, cf2[i].e3, &k3);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_crypto_fx_cf2");

	if (k3.keytype != cf2[i].e3)
	    krb5_errx(context, 1, "length not right");
	if (k3.keyvalue.length != cf2[i].len ||
	    memcmp(k3.keyvalue.data, cf2[i].key, cf2[i].len) != 0)
	    krb5_errx(context, 1, "key not same");

	krb5_crypto_destroy(context, c1);
	krb5_crypto_destroy(context, c2);

	krb5_free_keyblock_contents(context, &k1);
	krb5_free_keyblock_contents(context, &k2);
	krb5_free_keyblock_contents(context, &k3);
    }
}

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
		    "");
    exit (ret);
}

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;
    int optidx = 0;

    setprogname(argv[0]);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    test_cf2(context);

    krb5_free_context(context);

    return 0;
}
