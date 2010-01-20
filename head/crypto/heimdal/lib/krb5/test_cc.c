/*
 * Copyright (c) 2003 - 2007 Kungliga Tekniska Högskolan
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
#include <getarg.h>
#include <err.h>

RCSID("$Id: test_cc.c 22115 2007-12-03 21:21:42Z lha $");

static int debug_flag	= 0;
static int version_flag = 0;
static int help_flag	= 0;

static void
test_default_name(krb5_context context)
{
    krb5_error_code ret;
    const char *p, *test_cc_name = "/tmp/krb5-cc-test-foo";
    char *p1, *p2, *p3;

    p = krb5_cc_default_name(context);
    if (p == NULL)
	krb5_errx (context, 1, "krb5_cc_default_name 1 failed");
    p1 = estrdup(p);

    ret = krb5_cc_set_default_name(context, NULL);
    if (p == NULL)
	krb5_errx (context, 1, "krb5_cc_set_default_name failed");

    p = krb5_cc_default_name(context);
    if (p == NULL)
	krb5_errx (context, 1, "krb5_cc_default_name 2 failed");
    p2 = estrdup(p);

    if (strcmp(p1, p2) != 0)
	krb5_errx (context, 1, "krb5_cc_default_name no longer same");
	
    ret = krb5_cc_set_default_name(context, test_cc_name);
    if (p == NULL)
	krb5_errx (context, 1, "krb5_cc_set_default_name 1 failed");
    
    p = krb5_cc_default_name(context);
    if (p == NULL)
	krb5_errx (context, 1, "krb5_cc_default_name 2 failed");
    p3 = estrdup(p);
    
    if (strcmp(p3, test_cc_name) != 0)
	krb5_errx (context, 1, "krb5_cc_set_default_name 1 failed");

    free(p1);
    free(p2);
    free(p3);
}

/*
 * Check that a closed cc still keeps it data and that it's no longer
 * there when it's destroyed.
 */

static void
test_mcache(krb5_context context)
{
    krb5_error_code ret;
    krb5_ccache id, id2;
    const char *nc, *tc;
    char *c;
    krb5_principal p, p2;

    ret = krb5_parse_name(context, "lha@SU.SE", &p);
    if (ret)
	krb5_err(context, 1, ret, "krb5_parse_name");

    ret = krb5_cc_gen_new(context, &krb5_mcc_ops, &id);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_gen_new");

    ret = krb5_cc_initialize(context, id, p);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_initialize");

    nc = krb5_cc_get_name(context, id);
    if (nc == NULL)
	krb5_errx(context, 1, "krb5_cc_get_name");

    tc = krb5_cc_get_type(context, id);
    if (tc == NULL)
	krb5_errx(context, 1, "krb5_cc_get_name");

    asprintf(&c, "%s:%s", tc, nc);
    
    krb5_cc_close(context, id);
    
    ret = krb5_cc_resolve(context, c, &id2);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_resolve");

    ret = krb5_cc_get_principal(context, id2, &p2);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_get_principal");

    if (krb5_principal_compare(context, p, p2) == FALSE)
	krb5_errx(context, 1, "p != p2");

    krb5_cc_destroy(context, id2);
    krb5_free_principal(context, p);
    krb5_free_principal(context, p2);

    ret = krb5_cc_resolve(context, c, &id2);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_resolve");

    ret = krb5_cc_get_principal(context, id2, &p2);
    if (ret == 0)
	krb5_errx(context, 1, "krb5_cc_get_principal");

    krb5_cc_destroy(context, id2);
    free(c);
}

/*
 * Test that init works on a destroyed cc.
 */

static void
test_init_vs_destroy(krb5_context context, const krb5_cc_ops *ops)
{
    krb5_error_code ret;
    krb5_ccache id, id2;
    krb5_principal p, p2;
    char *n;

    ret = krb5_parse_name(context, "lha@SU.SE", &p);
    if (ret)
	krb5_err(context, 1, ret, "krb5_parse_name");

    ret = krb5_cc_gen_new(context, ops, &id);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_gen_new");

    asprintf(&n, "%s:%s",
	     krb5_cc_get_type(context, id),
	     krb5_cc_get_name(context, id));

    ret = krb5_cc_resolve(context, n, &id2);
    free(n);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_resolve");

    krb5_cc_destroy(context, id);

    ret = krb5_cc_initialize(context, id2, p);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_initialize");

    ret = krb5_cc_get_principal(context, id2, &p2);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_get_principal");

    krb5_cc_destroy(context, id2);
    krb5_free_principal(context, p);
    krb5_free_principal(context, p2);
}

static void
test_fcache_remove(krb5_context context)
{
    krb5_error_code ret;
    krb5_ccache id;
    krb5_principal p;
    krb5_creds cred;

    ret = krb5_parse_name(context, "lha@SU.SE", &p);
    if (ret)
	krb5_err(context, 1, ret, "krb5_parse_name");

    ret = krb5_cc_gen_new(context, &krb5_fcc_ops, &id);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_gen_new");

    ret = krb5_cc_initialize(context, id, p);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_initialize");

    /* */
    memset(&cred, 0, sizeof(cred));
    ret = krb5_parse_name(context, "krbtgt/SU.SE@SU.SE", &cred.server);
    if (ret)
	krb5_err(context, 1, ret, "krb5_parse_name");
    ret = krb5_parse_name(context, "lha@SU.SE", &cred.client);
    if (ret)
	krb5_err(context, 1, ret, "krb5_parse_name");

    ret = krb5_cc_store_cred(context, id, &cred);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_store_cred");

    ret = krb5_cc_remove_cred(context, id, 0, &cred);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_remove_cred");

    ret = krb5_cc_destroy(context, id);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_destroy");

    krb5_free_principal(context, p);
    krb5_free_principal(context, cred.server);
    krb5_free_principal(context, cred.client);
}

static void
test_mcc_default(void)
{
    krb5_context context;
    krb5_error_code ret;
    krb5_ccache id, id2;
    int i;

    for (i = 0; i < 10; i++) {

	ret = krb5_init_context(&context);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_init_context");

	ret = krb5_cc_set_default_name(context, "MEMORY:foo");
	if (ret)
	    krb5_err(context, 1, ret, "krb5_cc_set_default_name");

	ret = krb5_cc_default(context, &id);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_cc_default");

	ret = krb5_cc_default(context, &id2);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_cc_default");

	ret = krb5_cc_close(context, id);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_cc_close");

	ret = krb5_cc_close(context, id2);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_cc_close");

	krb5_free_context(context);
    }
}

struct {
    char *str;
    int fail;
    char *res;
} cc_names[] = {
    { "foo", 0, "foo" },
    { "%{uid}", 0 },
    { "foo%{null}", 0, "foo" },
    { "foo%{null}bar", 0, "foobar" },
    { "%{", 1 },
    { "%{foo %{", 1 },
    { "%{{", 1 },
};

static void
test_def_cc_name(krb5_context context)
{
    krb5_error_code ret;
    char *str;
    int i;

    for (i = 0; i < sizeof(cc_names)/sizeof(cc_names[0]); i++) {
	ret = _krb5_expand_default_cc_name(context, cc_names[i].str, &str);
	if (ret) {
	    if (cc_names[i].fail == 0)
		krb5_errx(context, 1, "test %d \"%s\" failed", 
			  i, cc_names[i].str);
	} else {
	    if (cc_names[i].fail)
		krb5_errx(context, 1, "test %d \"%s\" was successful", 
			  i, cc_names[i].str);
	    if (cc_names[i].res && strcmp(cc_names[i].res, str) != 0)
		krb5_errx(context, 1, "test %d %s != %s", 
			  i, cc_names[i].res, str);
	    if (debug_flag)
		printf("%s => %s\n", cc_names[i].str, str);
	    free(str);
	}
    }
}

static void
test_cache_find(krb5_context context, const char *type, const char *principal,
		int find)
{
    krb5_principal client;
    krb5_error_code ret;
    krb5_ccache id = NULL;

    ret = krb5_parse_name(context, principal, &client);
    if (ret)
	krb5_err(context, 1, ret, "parse_name for %s failed", principal);
    
    ret = krb5_cc_cache_match(context, client, type, &id);
    if (ret && find)
	krb5_err(context, 1, ret, "cc_cache_match for %s failed", principal);
    if (ret == 0 && !find)
	krb5_err(context, 1, ret, "cc_cache_match for %s found", principal);

    if (id)
	krb5_cc_close(context, id);
    krb5_free_principal(context, client);
}


static void
test_cache_iter(krb5_context context, const char *type, int destroy)
{
    krb5_cc_cache_cursor cursor;
    krb5_error_code ret;
    krb5_ccache id;
    
    ret = krb5_cc_cache_get_first (context, type, &cursor);
    if (ret == KRB5_CC_NOSUPP)
	return;
    else if (ret)
	krb5_err(context, 1, ret, "krb5_cc_cache_get_first(%s)", type);


    while ((ret = krb5_cc_cache_next (context, cursor, &id)) == 0) {
	krb5_principal principal;
	char *name;

	if (debug_flag)
	    printf("name: %s\n", krb5_cc_get_name(context, id));
	ret = krb5_cc_get_principal(context, id, &principal);
	if (ret == 0) {
	    ret = krb5_unparse_name(context, principal, &name);
	    if (ret == 0) {
		if (debug_flag)
		    printf("\tprincipal: %s\n", name);
		free(name);
	    }
	    krb5_free_principal(context, principal);
	}
	if (destroy)
	    krb5_cc_destroy(context, id);
	else
	    krb5_cc_close(context, id);
    }

    krb5_cc_cache_end_seq_get(context, cursor);
}

static void
test_copy(krb5_context context, const char *fromtype, const char *totype)
{
    const krb5_cc_ops *from, *to;
    krb5_ccache fromid, toid;
    krb5_error_code ret;
    krb5_principal p, p2;

    from = krb5_cc_get_prefix_ops(context, fromtype);
    if (from == NULL)
	krb5_errx(context, 1, "%s isn't a type", fromtype);

    to = krb5_cc_get_prefix_ops(context, totype);
    if (to == NULL)
	krb5_errx(context, 1, "%s isn't a type", totype);

    ret = krb5_parse_name(context, "lha@SU.SE", &p);
    if (ret)
	krb5_err(context, 1, ret, "krb5_parse_name");

    ret = krb5_cc_gen_new(context, from, &fromid);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_gen_new");

    ret = krb5_cc_initialize(context, fromid, p);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_initialize");

    ret = krb5_cc_gen_new(context, to, &toid);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_gen_new");

    ret = krb5_cc_copy_cache(context, fromid, toid);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_copy_cache");

    ret = krb5_cc_get_principal(context, toid, &p2);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_get_principal");

    if (krb5_principal_compare(context, p, p2) == FALSE)
	krb5_errx(context, 1, "p != p2");

    krb5_free_principal(context, p);
    krb5_free_principal(context, p2);

    krb5_cc_destroy(context, fromid);
    krb5_cc_destroy(context, toid);
}

static void
test_prefix_ops(krb5_context context, const char *name, const krb5_cc_ops *ops)
{
    const krb5_cc_ops *o;

    o = krb5_cc_get_prefix_ops(context, name);
    if (o == NULL)
	krb5_errx(context, 1, "found no match for prefix '%s'", name);
    if (strcmp(o->prefix, ops->prefix) != 0)
	krb5_errx(context, 1, "ops for prefix '%s' is not "
		  "the expected %s != %s", name, o->prefix, ops->prefix);
}


static struct getargs args[] = {
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
    int optidx = 0;
    krb5_ccache id1, id2;

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

    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    test_fcache_remove(context);
    test_default_name(context);
    test_mcache(context);
    test_init_vs_destroy(context, &krb5_mcc_ops);
    test_init_vs_destroy(context, &krb5_fcc_ops);
    test_mcc_default();
    test_def_cc_name(context);
    test_cache_iter(context, "MEMORY", 0);
    {
	krb5_principal p;
	krb5_cc_new_unique(context, "MEMORY", "bar", &id1);
	krb5_cc_new_unique(context, "MEMORY", "baz", &id2);
	krb5_parse_name(context, "lha@SU.SE", &p);
	krb5_cc_initialize(context, id1, p);
	krb5_free_principal(context, p);
    }

    test_cache_find(context, "MEMORY", "lha@SU.SE", 1);
    test_cache_find(context, "MEMORY", "hulabundulahotentot@SU.SE", 0);

    test_cache_iter(context, "MEMORY", 0);
    test_cache_iter(context, "MEMORY", 1);
    test_cache_iter(context, "MEMORY", 0);
    test_cache_iter(context, "FILE", 0);
    test_cache_iter(context, "API", 0);

    test_copy(context, "FILE", "FILE");
    test_copy(context, "MEMORY", "MEMORY");
    test_copy(context, "FILE", "MEMORY");
    test_copy(context, "MEMORY", "FILE");

    test_prefix_ops(context, "FILE:/tmp/foo", &krb5_fcc_ops);
    test_prefix_ops(context, "FILE", &krb5_fcc_ops);
    test_prefix_ops(context, "MEMORY", &krb5_mcc_ops);
    test_prefix_ops(context, "MEMORY:foo", &krb5_mcc_ops);
    test_prefix_ops(context, "/tmp/kaka", &krb5_fcc_ops);

    krb5_cc_destroy(context, id1);
    krb5_cc_destroy(context, id2);

    krb5_free_context(context);

    return 0;
}
