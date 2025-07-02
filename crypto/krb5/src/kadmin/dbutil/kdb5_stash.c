/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kadmin/dbutil/kdb5_stash.c - Store the master database key in a file */
/*
 * Copyright 1990 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "com_err.h"
#include <kadm5/admin.h>
#include <stdio.h>
#include "kdb5_util.h"

extern krb5_keyblock master_keyblock;
extern krb5_principal master_princ;
extern kadm5_config_params global_params;

extern int exit_status;
extern int close_policy_db;

void
kdb5_stash(argc, argv)
    int argc;
    char *argv[];
{
    extern char *optarg;
    extern int optind;
    int optchar;
    krb5_error_code retval;
    char *keyfile = 0;
    krb5_kvno mkey_kvno;

    keyfile = global_params.stash_file;

    optind = 1;
    while ((optchar = getopt(argc, argv, "f:")) != -1) {
        switch(optchar) {
        case 'f':
            keyfile = optarg;
            break;
        case '?':
        default:
            usage();
            return;
        }
    }

    if (!krb5_c_valid_enctype(master_keyblock.enctype)) {
        char tmp[32];
        if (krb5_enctype_to_name(master_keyblock.enctype, FALSE,
                                 tmp, sizeof(tmp)))
            com_err(progname, KRB5_PROG_KEYTYPE_NOSUPP,
                    _("while setting up enctype %d"), master_keyblock.enctype);
        else
            com_err(progname, KRB5_PROG_KEYTYPE_NOSUPP, "%s", tmp);
        exit_status++; return;
    }

    if (global_params.mask & KADM5_CONFIG_KVNO)
        mkey_kvno = global_params.kvno; /* user specified */
    else
        mkey_kvno = IGNORE_VNO; /* use whatever krb5_db_fetch_mkey finds */

    if (!valid_master_key) {
        /* TRUE here means read the keyboard, but only once */
        retval = krb5_db_fetch_mkey(util_context, master_princ,
                                    master_keyblock.enctype,
                                    TRUE, FALSE, (char *) NULL,
                                    &mkey_kvno,
                                    NULL, &master_keyblock);
        if (retval) {
            com_err(progname, retval, _("while reading master key"));
            exit_status++; return;
        }

        retval = krb5_db_fetch_mkey_list(util_context, master_princ,
                                         &master_keyblock);
        if (retval) {
            com_err(progname, retval, _("while getting master key list"));
            exit_status++; return;
        }
    } else {
        printf(_("Using existing stashed keys to update stash file.\n"));
    }

    retval = krb5_db_store_master_key_list(util_context, keyfile, master_princ,
                                           NULL);
    if (retval) {
        com_err(progname, retval, _("while storing key"));
        exit_status++; return;
    }

    exit_status = 0;
    return;
}
