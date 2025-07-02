/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * This driver routine is used to test many of the standard Kerberos library
 * routines.
 */

#include "autoconf.h"
#include "k5-int.h"
#include <time.h>

#include "com_err.h"

void test_string_to_timestamp (krb5_context, char *);
void test_425_conv_principal (krb5_context, char *, char*, char *);
void test_524_conv_principal (krb5_context, char *);
void test_parse_name (krb5_context, const char *);
void test_name_type (krb5_context, const char *);
void test_set_realm (krb5_context, const char *, const char *);
void usage (char *);

void
test_string_to_timestamp(krb5_context ctx, char *ktime)
{
    krb5_timestamp      timestamp;
    time_t              t;
    krb5_error_code     retval;

    retval = krb5_string_to_timestamp(ktime, &timestamp);
    if (retval) {
        com_err("krb5_string_to_timestamp", retval, 0);
        return;
    }
    t = ts2tt(timestamp);
    printf("Parsed time was %s", ctime(&t));
}

void
test_425_conv_principal(krb5_context ctx, char *name, char *inst, char *realm)
{
    krb5_error_code     retval;
    krb5_principal      princ;
    char                *out_name;

    retval = krb5_425_conv_principal(ctx, name, inst, realm, &princ);
    if (retval) {
        com_err("krb5_425_conv_principal", retval, 0);
        return;
    }
    retval = krb5_unparse_name(ctx, princ, &out_name);
    if (retval) {
        com_err("krb5_unparse_name", retval, 0);
        return;
    }
    printf("425_converted principal(%s, %s, %s): '%s'\n",
           name, inst, realm, out_name);
    free(out_name);
    krb5_free_principal(ctx, princ);
}

void
test_524_conv_principal(krb5_context ctx, char *name)
{
    krb5_principal princ = 0;
    krb5_error_code retval;
#define ANAME_SZ 40
#define INST_SZ  40
#define REALM_SZ  40
    char aname[ANAME_SZ+1], inst[INST_SZ+1], realm[REALM_SZ+1];

    aname[ANAME_SZ] = inst[INST_SZ] = realm[REALM_SZ] = 0;
    retval = krb5_parse_name(ctx, name, &princ);
    if (retval) {
        com_err("krb5_parse_name", retval, 0);
        goto fail;
    }
    retval = krb5_524_conv_principal(ctx, princ, aname, inst, realm);
    if (retval) {
        com_err("krb5_524_conv_principal", retval, 0);
        goto fail;
    }
    printf("524_converted_principal(%s): '%s' '%s' '%s'\n",
           name, aname, inst, realm);
fail:
    if (princ)
        krb5_free_principal (ctx, princ);
}

void
test_parse_name(krb5_context ctx, const char *name)
{
    krb5_error_code retval;
    krb5_principal  princ = 0, princ2 = 0;
    char            *outname = 0;

    retval = krb5_parse_name(ctx, name, &princ);
    if (retval) {
        com_err("krb5_parse_name", retval, 0);
        goto fail;
    }
    retval = krb5_copy_principal(ctx, princ, &princ2);
    if (retval) {
        com_err("krb5_copy_principal", retval, 0);
        goto fail;
    }
    retval = krb5_unparse_name(ctx, princ2, &outname);
    if (retval) {
        com_err("krb5_unparse_name", retval, 0);
        goto fail;
    }
    printf("parsed (and unparsed) principal(%s): ", name);
    if (strcmp(name, outname) == 0)
        printf("MATCH\n");
    else
        printf("'%s'\n", outname);
fail:
    if (outname)
        free(outname);
    if (princ)
        krb5_free_principal(ctx, princ);
    if (princ2)
        krb5_free_principal(ctx, princ2);
}

void
test_name_type(krb5_context ctx, const char *name)
{
    krb5_error_code retval;
    krb5_principal  princ = 0;

    retval = krb5_parse_name(ctx, name, &princ);
    if (retval) {
        com_err("krb5_parse_name", retval, 0);
        return;
    }
    printf("name_type principal(%s): %d\n", name, krb5_princ_type(ctx, princ));
    krb5_free_principal(ctx, princ);
}

void
test_set_realm(krb5_context ctx, const char *name, const char *realm)
{
    krb5_error_code retval;
    krb5_principal  princ = 0;
    char            *outname = 0;

    retval = krb5_parse_name(ctx, name, &princ);
    if (retval) {
        com_err("krb5_parse_name", retval, 0);
        goto fail;
    }
    retval = krb5_set_principal_realm(ctx, princ, realm);
    if (retval) {
        com_err("krb5_set_principal_realm", retval, 0);
        goto fail;
    }
    retval = krb5_unparse_name(ctx, princ, &outname);
    if (retval) {
        com_err("krb5_unparse_name", retval, 0);
        goto fail;
    }
    printf("old principal: %s, modified principal: %s\n", name,
           outname);
fail:
    if (outname)
        free(outname);
    if (princ)
        krb5_free_principal(ctx, princ);
}

void
usage(char *progname)
{
    fprintf(stderr, "%s: Usage: %s 425_conv_principal <name> <inst> <realm>\n",
            progname, progname);
    fprintf(stderr, "\t%s 524_conv_principal <name>\n", progname);
    fprintf(stderr, "\t%s parse_name <name>\n", progname);
    fprintf(stderr, "\t%s name_type <name>\n", progname);
    fprintf(stderr, "\t%s set_realm <name> <realm>\n", progname);
    fprintf(stderr, "\t%s string_to_timestamp <time>\n", progname);
    exit(1);
}

int
main(int argc, char **argv)
{
    krb5_context ctx;
    krb5_error_code retval;
    char *progname;
    char *name, *inst, *realm;

    retval = krb5_init_context(&ctx);
    if (retval) {
        fprintf(stderr, "krb5_init_context returned error %ld\n",
                (long) retval);
        exit(1);
    }
    progname = argv[0];

    /* Parse arguments. */
    argc--; argv++;
    while (argc) {
        if (strcmp(*argv, "425_conv_principal") == 0) {
            argc--; argv++;
            if (!argc) usage(progname);
            name = *argv;
            argc--; argv++;
            if (!argc) usage(progname);
            inst = *argv;
            argc--; argv++;
            if (!argc) usage(progname);
            realm = *argv;
            test_425_conv_principal(ctx, name, inst, realm);
        } else if (strcmp(*argv, "parse_name") == 0) {
            argc--; argv++;
            if (!argc) usage(progname);
            name = *argv;
            test_parse_name(ctx, name);
        } else if (strcmp(*argv, "name_type") == 0) {
            argc--; argv++;
            if (!argc) usage(progname);
            name = *argv;
            test_name_type(ctx, name);
        } else if (strcmp(*argv, "set_realm") == 0) {
            argc--; argv++;
            if (!argc) usage(progname);
            name = *argv;
            argc--; argv++;
            if (!argc) usage(progname);
            realm = *argv;
            test_set_realm(ctx, name, realm);
        } else if (strcmp(*argv, "string_to_timestamp") == 0) {
            argc--; argv++;
            if (!argc) usage(progname);
            test_string_to_timestamp(ctx, *argv);
        } else if (strcmp(*argv, "524_conv_principal") == 0) {
            argc--; argv++;
            if (!argc) usage(progname);
            test_524_conv_principal(ctx, *argv);
        }
        else
            usage(progname);
        argc--; argv++;
    }

    krb5_free_context(ctx);

    return 0;
}
