/*
 * Copyright (c) 2006 - 2007 Kungliga Tekniska Högskolan
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

#include "hx_locl.h"
RCSID("$Id: test_name.c 19882 2007-01-13 01:02:57Z lha $");

static int
test_name(hx509_context context, const char *name)
{
    hx509_name n;
    char *s;
    int ret;

    ret = hx509_parse_name(context, name, &n);
    if (ret)
	return 1;

    ret = hx509_name_to_string(n, &s);
    if (ret)
	return 1;

    if (strcmp(s, name) != 0)
	return 1;

    hx509_name_free(&n);
    free(s);

    return 0;
}

static int
test_name_fail(hx509_context context, const char *name)
{
    hx509_name n;

    if (hx509_parse_name(context, name, &n) == HX509_NAME_MALFORMED)
	return 0;
    hx509_name_free(&n);
    return 1;
}

static int
test_expand(hx509_context context, const char *name, const char *expected)
{
    hx509_env env;
    hx509_name n;
    char *s;
    int ret;

    hx509_env_init(context, &env);
    hx509_env_add(context, env, "uid", "lha");

    ret = hx509_parse_name(context, name, &n);
    if (ret)
	return 1;

    ret = hx509_name_expand(context, n, env);
    hx509_env_free(&env);
    if (ret)
	return 1;

    ret = hx509_name_to_string(n, &s);
    hx509_name_free(&n);
    if (ret)
	return 1;
    
    ret = strcmp(s, expected) != 0;
    free(s);
    if (ret)
	return 1;

    return 0;
}

int
main(int argc, char **argv)
{
    hx509_context context;
    int ret = 0;

    ret = hx509_context_init(&context);
    if (ret)
	errx(1, "hx509_context_init failed with %d", ret);

    ret += test_name(context, "CN=foo,C=SE");
    ret += test_name(context, "CN=foo,CN=kaka,CN=FOO,DC=ad1,C=SE");
    ret += test_name(context, "1.2.3.4=foo,C=SE");
    ret += test_name_fail(context, "=");
    ret += test_name_fail(context, "CN=foo,=foo");
    ret += test_name_fail(context, "CN=foo,really-unknown-type=foo");

    ret += test_expand(context, "UID=${uid},C=SE", "UID=lha,C=SE");
    ret += test_expand(context, "UID=foo${uid},C=SE", "UID=foolha,C=SE");
    ret += test_expand(context, "UID=${uid}bar,C=SE", "UID=lhabar,C=SE");
    ret += test_expand(context, "UID=f${uid}b,C=SE", "UID=flhab,C=SE");
    ret += test_expand(context, "UID=${uid}${uid},C=SE", "UID=lhalha,C=SE");
    ret += test_expand(context, "UID=${uid}{uid},C=SE", "UID=lha{uid},C=SE");

    hx509_context_free(&context);

    return ret;
}
