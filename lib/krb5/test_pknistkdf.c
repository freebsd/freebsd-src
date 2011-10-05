/*
 * Copyright (c) 2008 Kungliga Tekniska Högskolan
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
#include <pkinit_asn1.h>
#include <err.h>
#include <getarg.h>
#include <hex.h>

static int verbose_flag = 0;

struct testcase {
    const heim_oid *oid;
    krb5_data Z;
    const char *client;
    const char *server;
    krb5_enctype enctype;
    krb5_data as_req;
    krb5_data pk_as_rep;
    krb5_data ticket;

    krb5_data key;
} tests[] = {
    /* 0 */
    {
        NULL,                            /* AlgorithmIdentifier */
	{ /* Z */
	    256,
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	},
	"lha@SU.SE", /* client, partyUInfo */
	"krbtgt/SU.SE@SU.SE", /* server, partyVInfo */
	ETYPE_AES256_CTS_HMAC_SHA1_96, /* enctype */
	{ /* as_req */
	    10,
	    "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	},
	{ /* pk_as_rep */
	    9,
	    "\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB"
	},
	{ /* ticket */
	    55,
	    "\x61\x35\x30\x33\xa0\x03\x02\x01\x05\xa1\x07\x1b\x05\x53\x55\x2e"
	    "\x53\x45\xa2\x10\x30\x0e\xa0\x03\x02\x01\x01\xa1\x07\x30\x05\x1b"
	    "\x03\x6c\x68\x61\xa3\x11\x30\x0f\xa0\x03\x02\x01\x12\xa2\x08\x04"
	    "\x06\x68\x65\x6a\x68\x65\x6a"
	},
	{ /* key */
	    32,
	    "\xc7\x62\x89\xec\x4b\x28\xa6\x91\xff\xce\x80\xbb\xb7\xec\x82\x41"
	    "\x52\x3f\x99\xb1\x90\xcf\x2d\x34\x8f\x54\xa8\x65\x81\x2c\x32\x73"
	}
    },
    /* 1 */
    {
        NULL,                            /* AlgorithmIdentifier */
	{ /* Z */
	    256,
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	},
	"lha@SU.SE", /* client, partyUInfo */
	"krbtgt/SU.SE@SU.SE", /* server, partyVInfo */
	ETYPE_AES256_CTS_HMAC_SHA1_96, /* enctype */
	{ /* as_req */
	    10,
	    "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	},
	{ /* pk_as_rep */
	    9,
	    "\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB"
	},
	{ /* ticket */
	    55,
	    "\x61\x35\x30\x33\xa0\x03\x02\x01\x05\xa1\x07\x1b\x05\x53\x55\x2e"
	    "\x53\x45\xa2\x10\x30\x0e\xa0\x03\x02\x01\x01\xa1\x07\x30\x05\x1b"
	    "\x03\x6c\x68\x61\xa3\x11\x30\x0f\xa0\x03\x02\x01\x12\xa2\x08\x04"
	    "\x06\x68\x65\x6a\x68\x65\x6a"
	},
	{ /* key */
	    32,
	    "\x59\xf3\xca\x77\x5b\x20\x17\xe9\xad\x36\x3f\x47\xca\xbd\x43\xb8"
	    "\x8c\xb8\x90\x35\x8d\xc6\x0d\x52\x0d\x11\x9f\xb0\xdc\x24\x0b\x61"
	}
    },
    /* 2 */
    {
        NULL,                            /* AlgorithmIdentifier */
	{ /* Z */
	    256,
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	},
	"lha@SU.SE", /* client, partyUInfo */
	"krbtgt/SU.SE@SU.SE", /* server, partyVInfo */
	ETYPE_AES256_CTS_HMAC_SHA1_96, /* enctype */
	{ /* as_req */
	    10,
	    "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	},
	{ /* pk_as_rep */
	    9,
	    "\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB"
	},
	{ /* ticket */
	    55,
	    "\x61\x35\x30\x33\xa0\x03\x02\x01\x05\xa1\x07\x1b\x05\x53\x55\x2e"
	    "\x53\x45\xa2\x10\x30\x0e\xa0\x03\x02\x01\x01\xa1\x07\x30\x05\x1b"
	    "\x03\x6c\x68\x61\xa3\x11\x30\x0f\xa0\x03\x02\x01\x12\xa2\x08\x04"
	    "\x06\x68\x65\x6a\x68\x65\x6a"
	},
	{ /* key */
	    32,
	    "\x8a\x9a\xc5\x5f\x45\xda\x1a\x73\xd9\x1e\xe9\x88\x1f\xa9\x48\x81"
	    "\xce\xac\x66\x2d\xb1\xd3\xb9\x0a\x9d\x0e\x52\x83\xdf\xe1\x84\x3d"
	}
    }
};

#ifdef MAKETICKET
static void
fooTicket(void)
{
    krb5_error_code ret;
    krb5_data data;
    size_t size;
    Ticket t;

    t.tkt_vno = 5;
    t.realm = "SU.SE";
    t.sname.name_type = KRB5_NT_PRINCIPAL;
    t.sname.name_string.len = 1;
    t.sname.name_string.val = ecalloc(1, sizeof(t.sname.name_string.val[0]));
    t.sname.name_string.val[0] = estrdup("lha");
    t.enc_part.etype = ETYPE_AES256_CTS_HMAC_SHA1_96;
    t.enc_part.kvno = NULL;
    t.enc_part.cipher.length = 6;
    t.enc_part.cipher.data = "hejhej";

    ASN1_MALLOC_ENCODE(Ticket, data.data, data.length, &t, &size, ret);
    if (ret)
	errx(1, "ASN1_MALLOC_ENCODE(Ticket)");

    rk_dumpdata("foo", data.data, data.length);
    free(data.data);
}
#endif

static void
test_dh2key(krb5_context context, int i, struct testcase *c)
{
    krb5_error_code ret;
    krb5_keyblock key;
    krb5_principal client, server;
    Ticket ticket;
    AlgorithmIdentifier ai;
    size_t size;

    memset(&ticket, 0, sizeof(&ticket));

    ai.algorithm = *c->oid;
    ai.parameters = NULL;

    ret = decode_Ticket(c->ticket.data, c->ticket.length, &ticket, &size);
    if (ret)
	krb5_errx(context, 1, "decode ticket: %d", ret);

    ret = krb5_parse_name(context, c->client, &client);
    if (ret)
	krb5_err(context, 1, ret, "parse_name: %s", c->client);
    ret = krb5_parse_name(context, c->server, &server);
    if (ret)
	krb5_err(context, 1, ret, "parse_name: %s", c->server);

    if (verbose_flag) {
	char *str;
	hex_encode(c->Z.data, c->Z.length, &str);
	printf("Z: %s\n", str);
	free(str);
	printf("client: %s\n", c->client);
	printf("server: %s\n", c->server);
	printf("enctype: %d\n", (int)c->enctype);
	hex_encode(c->as_req.data, c->as_req.length, &str);
	printf("as-req: %s\n", str);
	free(str);
	hex_encode(c->pk_as_rep.data, c->pk_as_rep.length, &str);
	printf("pk-as-rep: %s\n", str);
	free(str);
	hex_encode(c->ticket.data, c->ticket.length, &str);
	printf("ticket: %s\n", str);
	free(str);
    }

    ret = _krb5_pk_kdf(context,
		       &ai,
		       c->Z.data,
		       c->Z.length,
		       client,
		       server,
		       c->enctype,
		       &c->as_req,
		       &c->pk_as_rep,
		       &ticket,
		       &key);
    krb5_free_principal(context, client);
    krb5_free_principal(context, server);
    if (ret)
	krb5_err(context, 1, ret, "_krb5_pk_kdf: %d", i);

    if (verbose_flag) {
	char *str;
	hex_encode(key.keyvalue.data, key.keyvalue.length, &str);
	printf("key: %s\n", str);
	free(str);
    }

    if (key.keyvalue.length != c->key.length ||
	memcmp(key.keyvalue.data, c->key.data, c->key.length) != 0)
	krb5_errx(context, 1, "resulting key wrong: %d", i);

    krb5_free_keyblock_contents(context, &key);
    free_Ticket(&ticket);
}




static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"verbose",	0,	arg_flag,	&verbose_flag,
     "verbose output", NULL },
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
    int i, optidx = 0;

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

#ifdef MAKETICKET
    fooTicket();
#endif

    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    tests[0].oid = &asn1_oid_id_pkinit_kdf_ah_sha1;
    tests[1].oid = &asn1_oid_id_pkinit_kdf_ah_sha256;
    tests[2].oid = &asn1_oid_id_pkinit_kdf_ah_sha512;

    for (i = 0; i < sizeof(tests)/sizeof(tests[0]); i++)
	test_dh2key(context, i, &tests[i]);

    krb5_free_context(context);

    return 0;
}
