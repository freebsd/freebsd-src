/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/t_kuserok.c - Test harness for krb5_kuserok */
/*
 * Copyright (C) 2010 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <krb5.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_principal principal;
    krb5_boolean ok;
    char *progname;

    progname = argv[0];
    if (argc != 3) {
        fprintf(stderr, "Usage: %s principal localuser\n", progname);
        return 1;
    }
    krb5_init_context(&context);
    ret = krb5_parse_name(context, argv[1], &principal);
    if (ret) {
        com_err(progname, ret, "while parsing principal name");
        return 1;
    }
    ok = krb5_kuserok(context, principal, argv[2]);
    printf("krb5_kuserok returns %s\n", ok ? "true" : "false");
    krb5_free_context(context);
    return 0;
}
