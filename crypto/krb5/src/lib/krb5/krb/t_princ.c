/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (c) 2003 Kungliga Tekniska Högskolan
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
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-int.h"

/*
 * Check that a closed cc still keeps it data and that it's no longer
 * there when it's destroyed.
 */

#if !defined(__cplusplus) && (__GNUC__ > 2)
static void err(krb5_context ctx, krb5_error_code code, const char *fmt, ...)
    __attribute__((__format__(__printf__, 3, 0)));
#endif

static void
err(krb5_context ctx, krb5_error_code code, const char *fmt, ...)
{
    va_list ap;
    char *msg;
    const char *errmsg = NULL;

    va_start(ap, fmt);
    if (vasprintf(&msg, fmt, ap) < 0)
        exit(1);
    va_end(ap);
    if (ctx && code)
        errmsg = krb5_get_error_message(ctx, code);
    if (errmsg)
        fprintf(stderr, "t_princ: %s: %s\n", msg, errmsg);
    else
        fprintf(stderr, "t_princ: %s\n", msg);
    exit(1);
}

static void
test_princ(krb5_context context)
{
    const char *princ = "lha@SU.SE";
    const char *princ_short = "lha";
    const char *noquote;
    krb5_error_code ret;
    char *princ_unparsed;
    char *princ_reformed = NULL;
    const char *realm;

    krb5_principal p, p2;

    ret = krb5_parse_name(context, princ, &p);
    if (ret)
        err(context, ret, "krb5_parse_name");

    ret = krb5_unparse_name(context, p, &princ_unparsed);
    if (ret)
        err(context, ret, "krb5_parse_name");

    if (strcmp(princ, princ_unparsed)) {
        err(context, 0, "%s != %s", princ, princ_unparsed);
    }

    free(princ_unparsed);

    ret = krb5_unparse_name_flags(context, p,
                                  KRB5_PRINCIPAL_UNPARSE_NO_REALM,
                                  &princ_unparsed);
    if (ret)
        err(context, ret, "krb5_parse_name");

    if (strcmp(princ_short, princ_unparsed))
        err(context, 0, "%s != %s", princ_short, princ_unparsed);
    free(princ_unparsed);

    realm = p->realm.data;

    asprintf(&princ_reformed, "%s@%s", princ_short, realm);

    ret = krb5_parse_name(context, princ_reformed, &p2);
    free(princ_reformed);
    if (ret)
        err(context, ret, "krb5_parse_name");

    if (!krb5_principal_compare(context, p, p2)) {
        err(context, 0, "p != p2");
    }

    krb5_free_principal(context, p2);

    ret = krb5_set_default_realm(context, "SU.SE");
    if (ret)
        err(context, ret, "krb5_parse_name");

    ret = krb5_unparse_name_flags(context, p,
                                  KRB5_PRINCIPAL_UNPARSE_SHORT,
                                  &princ_unparsed);
    if (ret)
        err(context, ret, "krb5_parse_name");

    if (strcmp(princ_short, princ_unparsed))
        err(context, 0, "'%s' != '%s'", princ_short, princ_unparsed);
    free(princ_unparsed);

    ret = krb5_parse_name(context, princ_short, &p2);
    if (ret)
        err(context, ret, "krb5_parse_name");

    if (!krb5_principal_compare(context, p, p2))
        err(context, 0, "p != p2");
    krb5_free_principal(context, p2);

    ret = krb5_unparse_name(context, p, &princ_unparsed);
    if (ret)
        err(context, ret, "krb5_parse_name");

    if (strcmp(princ, princ_unparsed))
        err(context, 0, "'%s' != '%s'", princ, princ_unparsed);
    free(princ_unparsed);

    ret = krb5_set_default_realm(context, "SAMBA.ORG");
    if (ret)
        err(context, ret, "krb5_parse_name");

    ret = krb5_parse_name(context, princ_short, &p2);
    if (ret)
        err(context, ret, "krb5_parse_name");

    if (krb5_principal_compare(context, p, p2))
        err(context, 0, "p == p2");

    if (!krb5_principal_compare_any_realm(context, p, p2))
        err(context, 0, "(ignoring realms) p != p2");

    ret = krb5_unparse_name(context, p2, &princ_unparsed);
    if (ret)
        err(context, ret, "krb5_parse_name");

    if (strcmp(princ, princ_unparsed) == 0)
        err(context, 0, "%s == %s", princ, princ_unparsed);
    free(princ_unparsed);

    krb5_free_principal(context, p2);

    ret = krb5_parse_name(context, princ, &p2);
    if (ret)
        err(context, ret, "krb5_parse_name");

    if (!krb5_principal_compare(context, p, p2))
        err(context, 0, "p != p2");

    ret = krb5_unparse_name(context, p2, &princ_unparsed);
    if (ret)
        err(context, ret, "krb5_parse_name");

    if (strcmp(princ, princ_unparsed))
        err(context, 0, "'%s' != '%s'", princ, princ_unparsed);
    free(princ_unparsed);

    krb5_free_principal(context, p2);

    ret = krb5_unparse_name_flags(context, p,
                                  KRB5_PRINCIPAL_UNPARSE_SHORT,
                                  &princ_unparsed);
    if (ret)
        err(context, ret, "krb5_unparse_name_short");

    if (strcmp(princ, princ_unparsed) != 0)
        err(context, 0, "'%s' != '%s'", princ, princ_unparsed);
    free(princ_unparsed);

    ret = krb5_unparse_name(context, p, &princ_unparsed);
    if (ret)
        err(context, ret, "krb5_unparse_name_short");

    if (strcmp(princ, princ_unparsed))
        err(context, 0, "'%s' != '%s'", princ, princ_unparsed);
    free(princ_unparsed);

    ret = krb5_parse_name_flags(context, princ,
                                KRB5_PRINCIPAL_PARSE_NO_REALM,
                                &p2);
    if (!ret)
        err(context, ret, "Should have failed to parse %s a "
            "short name", princ);

    ret = krb5_parse_name_flags(context, princ_short,
                                KRB5_PRINCIPAL_PARSE_NO_REALM,
                                &p2);
    if (ret)
        err(context, ret, "krb5_parse_name");

    ret = krb5_unparse_name_flags(context, p2,
                                  KRB5_PRINCIPAL_UNPARSE_NO_REALM,
                                  &princ_unparsed);
    krb5_free_principal(context, p2);
    if (ret)
        err(context, ret, "krb5_unparse_name_norealm");

    if (strcmp(princ_short, princ_unparsed))
        err(context, 0, "'%s' != '%s'", princ_short, princ_unparsed);
    free(princ_unparsed);

    ret = krb5_parse_name_flags(context, princ_short,
                                KRB5_PRINCIPAL_PARSE_REQUIRE_REALM,
                                &p2);
    if (!ret)
        err(context, ret, "Should have failed to parse %s "
            "because it lacked a realm", princ_short);

    ret = krb5_parse_name_flags(context, princ,
                                KRB5_PRINCIPAL_PARSE_REQUIRE_REALM,
                                &p2);
    if (ret)
        err(context, ret, "krb5_parse_name");

    if (!krb5_principal_compare(context, p, p2))
        err(context, 0, "p != p2");

    ret = krb5_unparse_name_flags(context, p2,
                                  KRB5_PRINCIPAL_UNPARSE_NO_REALM,
                                  &princ_unparsed);
    krb5_free_principal(context, p2);
    if (ret)
        err(context, ret, "krb5_unparse_name_norealm");

    if (strcmp(princ_short, princ_unparsed))
        err(context, 0, "'%s' != '%s'", princ_short, princ_unparsed);
    free(princ_unparsed);

    krb5_free_principal(context, p);

    /* test quoting */

    princ = "test\\/principal@SU.SE";
    noquote = "test/principal@SU.SE";

    ret = krb5_parse_name_flags(context, princ, 0, &p);
    if (ret)
        err(context, ret, "krb5_parse_name");

    ret = krb5_unparse_name_flags(context, p, 0, &princ_unparsed);
    if (ret)
        err(context, ret, "krb5_unparse_name_flags");

    if (strcmp(princ, princ_unparsed))
        err(context, 0, "q '%s' != '%s'", princ, princ_unparsed);
    free(princ_unparsed);

    ret = krb5_unparse_name_flags(context, p, KRB5_PRINCIPAL_UNPARSE_DISPLAY,
                                  &princ_unparsed);
    if (ret)
        err(context, ret, "krb5_unparse_name_flags");

    if (strcmp(noquote, princ_unparsed))
        err(context, 0, "nq '%s' != '%s'", noquote, princ_unparsed);
    free(princ_unparsed);

    krb5_free_principal(context, p);
}

static void
test_enterprise(krb5_context context)
{
    krb5_error_code ret;
    char *unparsed;
    krb5_principal p;

    ret = krb5_set_default_realm(context, "SAMBA.ORG");
    if (ret)
        err(context, ret, "krb5_parse_name");

    ret = krb5_parse_name_flags(context, "lha@su.se@WIN.SU.SE",
                                KRB5_PRINCIPAL_PARSE_ENTERPRISE, &p);
    if (ret)
        err(context, ret, "krb5_parse_name_flags");

    ret = krb5_unparse_name(context, p, &unparsed);
    if (ret)
        err(context, ret, "krb5_unparse_name");

    krb5_free_principal(context, p);

    if (strcmp(unparsed, "lha\\@su.se@WIN.SU.SE") != 0)
        err(context, 0, "enterprise name failed 1");
    free(unparsed);

    /*
     *
     */

    ret = krb5_parse_name_flags(context, "lha\\@su.se@WIN.SU.SE",
                                KRB5_PRINCIPAL_PARSE_ENTERPRISE, &p);
    if (ret)
        err(context, ret, "krb5_parse_name_flags");

    ret = krb5_unparse_name(context, p, &unparsed);
    if (ret)
        err(context, ret, "krb5_unparse_name");

    krb5_free_principal(context, p);
    if (strcmp(unparsed, "lha\\@su.se\\@WIN.SU.SE@SAMBA.ORG") != 0)
        err(context, 0, "enterprise name failed 2: %s", unparsed);
    free(unparsed);

    /*
     *
     */

    ret = krb5_parse_name_flags(context, "lha\\@su.se@WIN.SU.SE", 0, &p);
    if (ret)
        err(context, ret, "krb5_parse_name_flags");

    ret = krb5_unparse_name(context, p, &unparsed);
    if (ret)
        err(context, ret, "krb5_unparse_name");

    krb5_free_principal(context, p);
    if (strcmp(unparsed, "lha\\@su.se@WIN.SU.SE") != 0)
        err(context, 0, "enterprise name failed 3");
    free(unparsed);

    /*
     *
     */

    ret = krb5_parse_name_flags(context, "lha@su.se",
                                KRB5_PRINCIPAL_PARSE_ENTERPRISE, &p);
    if (ret)
        err(context, ret, "krb5_parse_name_flags");

    ret = krb5_unparse_name(context, p, &unparsed);
    if (ret)
        err(context, ret, "krb5_unparse_name");

    krb5_free_principal(context, p);
    if (strcmp(unparsed, "lha\\@su.se@SAMBA.ORG") != 0)
        err(context, 0, "enterprise name failed 2: %s", unparsed);
    free(unparsed);


    ret = krb5_parse_name_flags(context, "lukeh@ntdev.padl.com",
                                KRB5_PRINCIPAL_PARSE_ENTERPRISE, &p);
    if (ret)
        err(context, ret, "krb5_parse_name_flags");

    ret = krb5_unparse_name_flags(context, p, KRB5_PRINCIPAL_UNPARSE_NO_REALM,
                                  &unparsed);
    if (ret)
        err(context, ret, "krb5_unparse_name");

    krb5_free_principal(context, p);
    if (strcmp(unparsed, "lukeh@ntdev.padl.com") != 0)
        err(context, 0, "enterprise name failed 4: %s", unparsed);
    free(unparsed);
}


int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;

    ret = krb5_init_context(&context);
    if (ret)
        err(NULL, 0, "krb5_init_context failed: %d", ret);

    test_princ(context);

    test_enterprise(context);

    krb5_free_context(context);

    return 0;
}
