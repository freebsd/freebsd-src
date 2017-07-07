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
#include <locale.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>

extern int optind;
extern char *optarg;

static char *prog;

static void xusage()
{
    fprintf(stderr, _("usage: %s [-C] [-u] [-c ccache] [-e etype]\n"), prog);
    fprintf(stderr, _("\t[-k keytab] [-S sname] [-U for_user [-P]]\n"));
    fprintf(stderr, _("\tservice1 service2 ...\n"));
    exit(1);
}

int quiet = 0;

static void do_v5_kvno (int argc, char *argv[],
                        char *ccachestr, char *etypestr, char *keytab_name,
                        char *sname, int canon, int unknown,
                        char *for_user, int proxy);

#include <com_err.h>
static void extended_com_err_fn (const char *, errcode_t, const char *,
                                 va_list);

int main(int argc, char *argv[])
{
    int option;
    char *etypestr = NULL, *ccachestr = NULL, *keytab_name = NULL;
    char *sname = NULL, *for_user = NULL;
    int canon = 0, unknown = 0, proxy = 0;

    setlocale(LC_ALL, "");
    set_com_err_hook (extended_com_err_fn);

    prog = strrchr(argv[0], '/');
    prog = prog ? (prog + 1) : argv[0];

    while ((option = getopt(argc, argv, "uCc:e:hk:qPS:U:")) != -1) {
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
            if (unknown == 1){
                fprintf(stderr,
                        _("Options -u and -S are mutually exclusive\n"));
                xusage();
            }
            break;
        case 'u':
            unknown = 1;
            if (sname){
                fprintf(stderr,
                        _("Options -u and -S are mutually exclusive\n"));
                xusage();
            }
            break;
        case 'U':
            for_user = optarg; /* S4U2Self - protocol transition */
            break;
        default:
            xusage();
            break;
        }
    }

    if (proxy) {
        if (keytab_name == NULL) {
            fprintf(stderr, _("Option -P (constrained delegation) "
                              "requires keytab to be specified\n"));
            xusage();
        } else if (for_user == NULL) {
            fprintf(stderr, _("Option -P (constrained delegation) requires "
                              "option -U (protocol transition)\n"));
            xusage();
        }
    }

    if ((argc - optind) < 1)
        xusage();

    do_v5_kvno(argc - optind, argv + optind,
               ccachestr, etypestr, keytab_name, sname,
               canon, unknown, for_user, proxy);
    return 0;
}

#include <k5-int.h>
static krb5_context context;
static void extended_com_err_fn (const char *myprog, errcode_t code,
                                 const char *fmt, va_list args)
{
    const char *emsg;
    emsg = krb5_get_error_message (context, code);
    fprintf (stderr, "%s: %s ", myprog, emsg);
    krb5_free_error_message (context, emsg);
    vfprintf (stderr, fmt, args);
    fprintf (stderr, "\n");
}

static void do_v5_kvno (int count, char *names[],
                        char * ccachestr, char *etypestr, char *keytab_name,
                        char *sname, int canon, int unknown, char *for_user,
                        int proxy)
{
    krb5_error_code ret;
    int i, errors;
    krb5_enctype etype;
    krb5_ccache ccache;
    krb5_principal me;
    krb5_creds in_creds;
    krb5_keytab keytab = NULL;
    krb5_principal for_user_princ = NULL;
    krb5_flags options;

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

    if (keytab_name) {
        ret = krb5_kt_resolve(context, keytab_name, &keytab);
        if (ret) {
            com_err(prog, ret, _("resolving keytab %s"), keytab_name);
            exit(1);
        }
    }

    if (for_user) {
        ret = krb5_parse_name_flags(context, for_user,
                                    KRB5_PRINCIPAL_PARSE_ENTERPRISE,
                                    &for_user_princ);
        if (ret) {
            com_err(prog, ret, _("while parsing principal name %s"), for_user);
            exit(1);
        }
    }

    ret = krb5_cc_get_principal(context, ccache, &me);
    if (ret) {
        com_err(prog, ret, _("while getting client principal name"));
        exit(1);
    }

    errors = 0;

    options = 0;
    if (canon)
        options |= KRB5_GC_CANONICALIZE;

    for (i = 0; i < count; i++) {
        krb5_principal server = NULL;
        krb5_ticket *ticket = NULL;
        krb5_creds *out_creds = NULL;
        char *princ = NULL;

        memset(&in_creds, 0, sizeof(in_creds));

        if (sname != NULL) {
            ret = krb5_sname_to_principal(context, names[i],
                                          sname, KRB5_NT_SRV_HST,
                                          &server);
        } else {
            ret = krb5_parse_name(context, names[i], &server);
        }
        if (ret) {
            if (!quiet) {
                com_err(prog, ret, _("while parsing principal name %s"),
                        names[i]);
            }
            goto error;
        }
        if (unknown == 1) {
            krb5_princ_type(context, server) = KRB5_NT_UNKNOWN;
        }

        ret = krb5_unparse_name(context, server, &princ);
        if (ret) {
            com_err(prog, ret, _("while formatting parsed principal name for "
                                 "'%s'"), names[i]);
            goto error;
        }

        in_creds.keyblock.enctype = etype;

        if (for_user) {
            if (!proxy &&
                !krb5_principal_compare(context, me, server)) {
                com_err(prog, EINVAL,
                        _("client and server principal names must match"));
                goto error;
            }

            in_creds.client = for_user_princ;
            in_creds.server = me;

            ret = krb5_get_credentials_for_user(context, options, ccache,
                                                &in_creds, NULL, &out_creds);
        } else {
            in_creds.client = me;
            in_creds.server = server;
            ret = krb5_get_credentials(context, options, ccache,
                                       &in_creds, &out_creds);
        }

        if (ret) {
            com_err(prog, ret, _("while getting credentials for %s"), princ);
            goto error;
        }

        /* we need a native ticket */
        ret = krb5_decode_ticket(&out_creds->ticket, &ticket);
        if (ret) {
            com_err(prog, ret, _("while decoding ticket for %s"), princ);
            goto error;
        }

        if (keytab) {
            ret = krb5_server_decrypt_ticket_keytab(context, keytab, ticket);
            if (ret) {
                if (!quiet) {
                    fprintf(stderr, "%s: kvno = %d, keytab entry invalid\n",
                            princ, ticket->enc_part.kvno);
                }
                com_err(prog, ret, _("while decrypting ticket for %s"), princ);
                goto error;
            }
            if (!quiet) {
                printf(_("%s: kvno = %d, keytab entry valid\n"),
                       princ, ticket->enc_part.kvno);
            }
            if (proxy) {
                krb5_free_creds(context, out_creds);
                out_creds = NULL;

                in_creds.client = ticket->enc_part2->client;
                in_creds.server = server;

                ret = krb5_get_credentials_for_proxy(context,
                                                     KRB5_GC_CANONICALIZE,
                                                     ccache,
                                                     &in_creds,
                                                     ticket,
                                                     &out_creds);
                if (ret) {
                    com_err(prog, ret,
                            _("%s: constrained delegation failed"), princ);
                    goto error;
                }
            }
        } else {
            if (!quiet)
                printf(_("%s: kvno = %d\n"), princ, ticket->enc_part.kvno);
        }

        continue;

    error:
        if (server != NULL)
            krb5_free_principal(context, server);
        if (ticket != NULL)
            krb5_free_ticket(context, ticket);
        if (out_creds != NULL)
            krb5_free_creds(context, out_creds);
        if (princ != NULL)
            krb5_free_unparsed_name(context, princ);
        errors++;
    }

    if (keytab)
        krb5_kt_close(context, keytab);
    krb5_free_principal(context, me);
    krb5_free_principal(context, for_user_princ);
    krb5_cc_close(context, ccache);
    krb5_free_context(context);

    if (errors)
        exit(1);

    exit(0);
}
