/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/t_export_name.c - Test program for gss_export_name behavior */
/*
 * Copyright 2012 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
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

/*
 * Test program for gss_export_name, intended to be run from a Python test
 * script.  Imports a name, canonicalizes it to a mech, exports it,
 * re-imports/exports it to compare results, and then prints the hex form of
 * the exported name followed by a newline.
 *
 * Usage: ./t_export_name [-k|-s] user:username|krb5:princ|host:service@host
 *
 * The name is imported as a username, krb5 principal, or hostbased name.
 * By default or with -k, the name is canonicalized to the krb5 mech; -s
 * indicates SPNEGO instead.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void
usage(void)
{
    fprintf(stderr, "Usage: t_export_name [-k|-s] name\n");
    exit(1);
}

int
main(int argc, char *argv[])
{
    OM_uint32 minor, major;
    gss_OID mech = (gss_OID)gss_mech_krb5;
    gss_name_t name, mechname, impname;
    gss_buffer_desc buf, buf2;
    krb5_boolean use_composite = FALSE;
    gss_OID ntype;
    const char *name_arg;
    char opt;

    /* Parse arguments. */
    while (argc > 1 && argv[1][0] == '-') {
        opt = argv[1][1];
        argc--, argv++;
        if (opt == 'k')
            mech = &mech_krb5;
        else if (opt == 's')
            mech = &mech_spnego;
        else if (opt == 'c')
            use_composite = TRUE;
        else
            usage();
    }
    if (argc != 2)
        usage();
    name_arg = argv[1];

    /* Import the name. */
    name = import_name(name_arg);

    /* Canonicalize and export the name. */
    major = gss_canonicalize_name(&minor, name, mech, &mechname);
    check_gsserr("gss_canonicalize_name", major, minor);
    if (use_composite)
        major = gss_export_name_composite(&minor, mechname, &buf);
    else
        major = gss_export_name(&minor, mechname, &buf);
    check_gsserr("gss_export_name", major, minor);

    /* Import and re-export the name, and compare the results. */
    ntype = use_composite ? GSS_C_NT_COMPOSITE_EXPORT : GSS_C_NT_EXPORT_NAME;
    major = gss_import_name(&minor, &buf, ntype, &impname);
    check_gsserr("gss_import_name", major, minor);
    if (use_composite)
        major = gss_export_name_composite(&minor, impname, &buf2);
    else
        major = gss_export_name(&minor, impname, &buf2);
    check_gsserr("gss_export_name", major, minor);
    if (buf.length != buf2.length ||
        memcmp(buf.value, buf2.value, buf.length) != 0) {
        fprintf(stderr, "Mismatched results:\n");
        print_hex(stderr, &buf);
        print_hex(stderr, &buf2);
        return 1;
    }

    print_hex(stdout, &buf);

    (void)gss_release_name(&minor, &name);
    (void)gss_release_name(&minor, &mechname);
    (void)gss_release_name(&minor, &impname);
    (void)gss_release_buffer(&minor, &buf);
    (void)gss_release_buffer(&minor, &buf2);
    return 0;
}
