/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "k5-platform.h"
#include "k5-buf.h"
#include "k5-base64.h"
#include <locale.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <ctype.h>

static char *prog;
static int quiet = 0;

static void
xusage()
{
    fprintf(stderr, _("usage: %s [-c ccache] [-e etype] [-k keytab] [-q] "
                      "[-u | -S sname]\n"
                      "\t[[{-F cert_file | {-I | -U} for_user} [-P]] | "
                      "--u2u ccache]\n"
                      "\t[--cached-only] [--no-store] [--out-cache] "
                      "service1 service2 ...\n"),
            prog);
    exit(1);
}

static void do_v5_kvno(int argc, char *argv[], char *ccachestr, char *etypestr,
                       char *keytab_name, char *sname, int cached_only,
                       int canon, int no_store, int unknown, char *for_user,
                       int for_user_enterprise, char *for_user_cert_file,
                       int proxy, const char *out_ccname,
                       const char *u2u_ccname);

#include <com_err.h>
static void extended_com_err_fn(const char *myprog, errcode_t code,
                                const char *fmt, va_list args);

int
main(int argc, char *argv[])
{
    enum { OPTION_U2U = 256, OPTION_OUT_CACHE = 257 };
    const char *shopts = "uCc:e:hk:qPS:I:U:F:";
    int option;
    char *etypestr = NULL, *ccachestr = NULL, *keytab_name = NULL;
    char *sname = NULL, *for_user = NULL, *u2u_ccname = NULL;
    char *for_user_cert_file = NULL, *out_ccname = NULL;
    int canon = 0, unknown = 0, proxy = 0, for_user_enterprise = 0;
    int impersonate = 0, cached_only = 0, no_store = 0;
    struct option lopts[] = {
        { "cached-only", 0, &cached_only, 1 },
        { "no-store", 0, &no_store, 1 },
        { "out-cache", 1, NULL, OPTION_OUT_CACHE },
        { "u2u", 1, NULL, OPTION_U2U },
        { NULL, 0, NULL, 0 }
    };

    setlocale(LC_ALL, "");
    set_com_err_hook(extended_com_err_fn);

    prog = strrchr(argv[0], '/');
    prog = prog ? (prog + 1) : argv[0];

    while ((option = getopt_long(argc, argv, shopts, lopts, NULL)) != -1) {
        switch (option) {
        case 'C':
            canon = 1;
            break;
        case 'c':
            ccachestr = optarg;
            break;
        case 'e':
            etypestr = optarg;
            break;
        case 'h':
            xusage();
            break;
        case 'k':
            keytab_name = optarg;
            break;
        case 'q':
            quiet = 1;
            break;
        case 'P':
            proxy = 1; /* S4U2Proxy - constrained delegation */
            break;
        case 'S':
            sname = optarg;
            if (unknown == 1) {
                fprintf(stderr,
                        _("Options -u and -S are mutually exclusive\n"));
                xusage();
            }
            break;
        case 'u':
            unknown = 1;
            if (sname != NULL) {
                fprintf(stderr,
                        _("Options -u and -S are mutually exclusive\n"));
                xusage();
            }
            break;
        case 'I':
            impersonate = 1;
            for_user = optarg;
            break;
        case 'U':
            impersonate = 1;
            for_user_enterprise = 1;
            for_user = optarg;
            break;
        case 'F':
            impersonate = 1;
            for_user_cert_file = optarg;
            break;
        case OPTION_U2U:
            u2u_ccname = optarg;
            break;
        case OPTION_OUT_CACHE:
            out_ccname = optarg;
            break;
        case 0:
            /* If this option set a flag, do nothing else now. */
            break;
        default:
            xusage();
            break;
        }
    }

    if (u2u_ccname != NULL && impersonate) {
        fprintf(stderr,
                _("Options --u2u and -I|-U|-F are mutually exclusive\n"));
        xusage();
    }

    if (proxy) {
        if (!impersonate) {
            fprintf(stderr, _("Option -P (constrained delegation) requires "
                              "option -I|-U|-F (protocol transition)\n"));
            xusage();
        }
    }

    if (argc - optind < 1)
        xusage();

    do_v5_kvno(argc - optind, argv + optind, ccachestr, etypestr, keytab_name,
               sname, cached_only, canon, no_store, unknown, for_user,
               for_user_enterprise, for_user_cert_file, proxy, out_ccname,
               u2u_ccname);
    return 0;
}

#include <k5-int.h>
static krb5_context context;
static void extended_com_err_fn(const char *myprog, errcode_t code,
                                const char *fmt, va_list args)
{
    const char *emsg;

    emsg = krb5_get_error_message(context, code);
    fprintf(stderr, "%s: %s ", myprog, emsg);
    krb5_free_error_message(context, emsg);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

/* Read a line from fp into buf.  Trim any trailing whitespace, and return a
 * pointer to the first non-whitespace character. */
static const char *
read_line(FILE *fp, char *buf, size_t bufsize)
{
    char *end, *begin;

    if (fgets(buf, bufsize, fp) == NULL)
        return NULL;

    end = buf + strlen(buf);
    while (end > buf && isspace((uint8_t)end[-1]))
        *--end = '\0';

    begin = buf;
    while (isspace((uint8_t)*begin))
        begin++;

    return begin;
}

/* Read a certificate from file_name in PEM format, placing the DER
 * representation of the certificate in *der_out. */
static krb5_error_code
read_pem_file(char *file_name, krb5_data *der_out)
{
    krb5_error_code ret = 0;
    FILE *fp = NULL;
    const char *begin_line = "-----BEGIN CERTIFICATE-----";
    const char *end_line = "-----END ", *line;
    char linebuf[256], *b64;
    struct k5buf buf = EMPTY_K5BUF;
    uint8_t *der_cert;
    size_t dlen;

    *der_out = empty_data();

    fp = fopen(file_name, "r");
    if (fp == NULL)
        return errno;

    for (;;) {
        line = read_line(fp, linebuf, sizeof(linebuf));
        if (line == NULL) {
            ret = EINVAL;
            k5_setmsg(context, ret, _("No begin line not found"));
            goto cleanup;
        }
        if (strncmp(line, begin_line, strlen(begin_line)) == 0)
            break;
    }

    k5_buf_init_dynamic(&buf);
    for (;;) {
        line = read_line(fp, linebuf, sizeof(linebuf));
        if (line == NULL) {
            ret = EINVAL;
            k5_setmsg(context, ret, _("No end line found"));
            goto cleanup;
        }

        if (strncmp(line, end_line, strlen(end_line)) == 0)
            break;

        /* Header lines would be expected for an actual privacy-enhanced mail
         * message, but not for a certificate. */
        if (*line == '\0' || strchr(line, ':') != NULL) {
            ret = EINVAL;
            k5_setmsg(context, ret, _("Unexpected header line"));
            goto cleanup;
        }

        k5_buf_add(&buf, line);
    }

    b64 = k5_buf_cstring(&buf);
    if (b64 == NULL) {
        ret = ENOMEM;
        goto cleanup;
    }
    der_cert = k5_base64_decode(b64, &dlen);
    if (der_cert == NULL) {
        ret = EINVAL;
        k5_setmsg(context, ret, _("Invalid base64"));
        goto cleanup;
    }

    *der_out = make_data(der_cert, dlen);

cleanup:
    fclose(fp);
    k5_buf_free(&buf);
    return ret;
}

/* Request a single service ticket and display its status (unless quiet is
 * set).  On failure, display an error message and return non-zero. */
static krb5_error_code
kvno(const char *name, krb5_ccache ccache, krb5_principal me,
     krb5_enctype etype, krb5_keytab keytab, const char *sname,
     krb5_flags options, int unknown, krb5_principal for_user_princ,
     krb5_data *for_user_cert, int proxy, krb5_data *u2u_ticket,
     krb5_creds **creds_out)
{
    krb5_error_code ret;
    krb5_principal server = NULL;
    krb5_ticket *ticket = NULL;
    krb5_creds in_creds, *creds = NULL;
    char *princ = NULL;

    *creds_out = NULL;
    memset(&in_creds, 0, sizeof(in_creds));

    if (sname != NULL) {
        ret = krb5_sname_to_principal(context, name, sname, KRB5_NT_SRV_HST,
                                      &server);
    } else {
        ret = krb5_parse_name(context, name, &server);
    }
    if (ret) {
        if (!quiet)
            com_err(prog, ret, _("while parsing principal name %s"), name);
        goto cleanup;
    }
    if (unknown)
        krb5_princ_type(context, server) = KRB5_NT_UNKNOWN;

    ret = krb5_unparse_name(context, server, &princ);
    if (ret) {
        com_err(prog, ret, _("while formatting parsed principal name for "
                             "'%s'"), name);
        goto cleanup;
    }

    in_creds.keyblock.enctype = etype;

    if (u2u_ticket != NULL)
        in_creds.second_ticket = *u2u_ticket;

    if (for_user_princ != NULL || for_user_cert != NULL) {
        if (!proxy && !krb5_principal_compare(context, me, server)) {
            ret = EINVAL;
            com_err(prog, ret,
                    _("client and server principal names must match"));
            goto cleanup;
        }

        in_creds.client = for_user_princ;
        in_creds.server = me;
        ret = krb5_get_credentials_for_user(context, options, ccache,
                                            &in_creds, for_user_cert, &creds);
    } else {
        in_creds.client = me;
        in_creds.server = server;
        ret = krb5_get_credentials(context, options, ccache, &in_creds,
                                   &creds);
    }

    if (ret) {
        com_err(prog, ret, _("while getting credentials for %s"), princ);
        goto cleanup;
    }

    /* We need a native ticket. */
    ret = krb5_decode_ticket(&creds->ticket, &ticket);
    if (ret) {
        com_err(prog, ret, _("while decoding ticket for %s"), princ);
        goto cleanup;
    }

    if (keytab != NULL) {
        ret = krb5_server_decrypt_ticket_keytab(context, keytab, ticket);
        if (ret) {
            if (!quiet) {
                fprintf(stderr, "%s: kvno = %d, keytab entry invalid\n", princ,
                        ticket->enc_part.kvno);
            }
            com_err(prog, ret, _("while decrypting ticket for %s"), princ);
            goto cleanup;
        }
        if (!quiet) {
            printf(_("%s: kvno = %d, keytab entry valid\n"), princ,
                   ticket->enc_part.kvno);
        }
    } else {
        if (!quiet)
            printf(_("%s: kvno = %d\n"), princ, ticket->enc_part.kvno);
    }

    if (proxy) {
        in_creds.client = creds->client;
        creds->client = NULL;
        krb5_free_creds(context, creds);
        creds = NULL;
        in_creds.server = server;

        ret = krb5_get_credentials_for_proxy(context, KRB5_GC_CANONICALIZE,
                                             ccache, &in_creds, ticket,
                                             &creds);
        krb5_free_principal(context, in_creds.client);
        if (ret) {
            com_err(prog, ret, _("%s: constrained delegation failed"),
                    princ);
            goto cleanup;
        }
    }

    *creds_out = creds;
    creds = NULL;

cleanup:
    krb5_free_principal(context, server);
    krb5_free_ticket(context, ticket);
    krb5_free_creds(context, creds);
    krb5_free_unparsed_name(context, princ);
    return ret;
}

/* Fetch the encoded local TGT for ccname's default client principal. */
static krb5_error_code
get_u2u_ticket(const char *ccname, krb5_data **ticket_out)
{
    krb5_error_code ret;
    krb5_ccache cc = NULL;
    krb5_creds mcred, *creds = NULL;

    *ticket_out = NULL;
    memset(&mcred, 0, sizeof(mcred));

    ret = krb5_cc_resolve(context, ccname, &cc);
    if (ret)
        goto cleanup;
    ret = krb5_cc_get_principal(context, cc, &mcred.client);
    if (ret)
        goto cleanup;
    ret = krb5_build_principal_ext(context, &mcred.server,
                                   mcred.client->realm.length,
                                   mcred.client->realm.data,
                                   KRB5_TGS_NAME_SIZE, KRB5_TGS_NAME,
                                   mcred.client->realm.length,
                                   mcred.client->realm.data, 0);
    if (ret)
        goto cleanup;
    ret = krb5_get_credentials(context, KRB5_GC_CACHED, cc, &mcred, &creds);
    if (ret)
        goto cleanup;

    ret = krb5_copy_data(context, &creds->ticket, ticket_out);

cleanup:
    if (cc != NULL)
        krb5_cc_close(context, cc);
    krb5_free_cred_contents(context, &mcred);
    krb5_free_creds(context, creds);
    return ret;
}

static void
do_v5_kvno(int count, char *names[], char * ccachestr, char *etypestr,
           char *keytab_name, char *sname, int cached_only, int canon,
           int no_store, int unknown, char *for_user, int for_user_enterprise,
           char *for_user_cert_file, int proxy, const char *out_ccname,
           const char *u2u_ccname)
{
    krb5_error_code ret;
    int i, errors, flags, initialized = 0;
    krb5_enctype etype;
    krb5_ccache ccache, mcc, out_ccache = NULL;
    krb5_principal me;
    krb5_keytab keytab = NULL;
    krb5_principal for_user_princ = NULL;
    krb5_flags options = 0;
    krb5_data cert_data = empty_data(), *user_cert = NULL, *u2u_ticket = NULL;
    krb5_creds *creds;

    if (canon)
        options |= KRB5_GC_CANONICALIZE;
    if (cached_only)
        options |= KRB5_GC_CACHED;
    if (no_store || out_ccname != NULL)
        options |= KRB5_GC_NO_STORE;

    ret = krb5_init_context(&context);
    if (ret) {
        com_err(prog, ret, _("while initializing krb5 library"));
        exit(1);
    }

    if (etypestr) {
        ret = krb5_string_to_enctype(etypestr, &etype);
        if (ret) {
            com_err(prog, ret, _("while converting etype"));
            exit(1);
        }
    } else {
        etype = 0;
    }

    if (ccachestr)
        ret = krb5_cc_resolve(context, ccachestr, &ccache);
    else
        ret = krb5_cc_default(context, &ccache);
    if (ret) {
        com_err(prog, ret, _("while opening ccache"));
        exit(1);
    }

    if (out_ccname != NULL) {
        ret = krb5_cc_resolve(context, out_ccname, &out_ccache);
        if (ret) {
            com_err(prog, ret, _("while resolving output ccache"));
            exit(1);
        }
    }

    if (keytab_name != NULL) {
        ret = krb5_kt_resolve(context, keytab_name, &keytab);
        if (ret) {
            com_err(prog, ret, _("resolving keytab %s"), keytab_name);
            exit(1);
        }
    }

    if (for_user) {
        flags = for_user_enterprise ? KRB5_PRINCIPAL_PARSE_ENTERPRISE : 0;
        ret = krb5_parse_name_flags(context, for_user, flags, &for_user_princ);
        if (ret) {
            com_err(prog, ret, _("while parsing principal name %s"), for_user);
            exit(1);
        }
    }

    if (for_user_cert_file != NULL) {
        ret = read_pem_file(for_user_cert_file, &cert_data);
        if (ret) {
            com_err(prog, ret, _("while reading certificate file %s"),
                    for_user_cert_file);
            exit(1);
        }
        user_cert = &cert_data;
    }

    if (u2u_ccname != NULL) {
        ret = get_u2u_ticket(u2u_ccname, &u2u_ticket);
        if (ret) {
            com_err(prog, ret, _("while getting user-to-user ticket from %s"),
                    u2u_ccname);
            exit(1);
        }
        options |= KRB5_GC_USER_USER;
    }

    ret = krb5_cc_get_principal(context, ccache, &me);
    if (ret) {
        com_err(prog, ret, _("while getting client principal name"));
        exit(1);
    }

    if (out_ccache != NULL) {
        ret = krb5_cc_new_unique(context, "MEMORY", NULL, &mcc);
        if (ret) {
            com_err(prog, ret, _("while creating temporary output ccache"));
            exit(1);
        }
    }

    errors = 0;
    for (i = 0; i < count; i++) {
        if (kvno(names[i], ccache, me, etype, keytab, sname, options, unknown,
                 for_user_princ, user_cert, proxy, u2u_ticket, &creds) != 0) {
            errors++;
        } else if (out_ccache != NULL) {
            if (!initialized) {
                ret = krb5_cc_initialize(context, mcc, creds->client);
                if (ret) {
                    com_err(prog, ret, _("while initializing output ccache"));
                    exit(1);
                }
                initialized = 1;
            }
            if (count == 1)
                ret = k5_cc_store_primary_cred(context, mcc, creds);
            else
                ret = krb5_cc_store_cred(context, mcc, creds);
            if (ret) {
                com_err(prog, ret, _("while storing creds in output ccache"));
                exit(1);
            }
        }

        krb5_free_creds(context, creds);
    }

    if (!errors && out_ccache != NULL) {
        ret = krb5_cc_move(context, mcc, out_ccache);
        if (ret) {
            com_err(prog, ret, _("while writing output ccache"));
            exit(1);
        }
    }

    if (keytab != NULL)
        krb5_kt_close(context, keytab);
    krb5_free_principal(context, me);
    krb5_free_principal(context, for_user_princ);
    krb5_cc_close(context, ccache);
    krb5_free_data(context, u2u_ticket);
    krb5_free_data_contents(context, &cert_data);
    krb5_free_context(context);

    if (errors)
        exit(1);

    exit(0);
}
