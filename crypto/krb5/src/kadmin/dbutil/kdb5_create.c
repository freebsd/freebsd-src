/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kadmin/dbutil/kdb5_create.c - Create a KDC database */
/*
 * Copyright 1990,1991,2001, 2002, 2008 by the Massachusetts Institute of
 * Technology.  All Rights Reserved.
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
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <k5-int.h>
#include <kdb.h>
#include <kadm5/server_internal.h>
#include <kadm5/admin.h>
#include <adm_proto.h>
#include "kdb5_util.h"

enum ap_op {
    NULL_KEY,                           /* setup null keys */
    MASTER_KEY,                         /* use master key as new key */
    TGT_KEY                             /* special handling for tgt key */
};

struct realm_info {
    krb5_deltat max_life;
    krb5_deltat max_rlife;
    krb5_timestamp expiration;
    krb5_flags flags;
    krb5_keyblock *key;
    krb5_int32 nkslist;
    krb5_key_salt_tuple *kslist;
} rblock;

struct iterate_args {
    krb5_context        ctx;
    struct realm_info   *rblock;
    krb5_db_entry       *dbentp;
};

static krb5_error_code add_principal
(krb5_context,
 krb5_principal,
 enum ap_op,
 struct realm_info *);

/*
 * Steps in creating a database:
 *
 * 1) use the db calls to open/create a new database
 *
 * 2) get a realm name for the new db
 *
 * 3) get a master password for the new db; convert to an encryption key.
 *
 * 4) create various required entries in the database
 *
 * 5) close & exit
 */

extern krb5_keyblock master_keyblock;
extern krb5_principal master_princ;
extern char *mkey_fullname;
krb5_data master_salt;

krb5_data tgt_princ_entries[] = {
    {0, KRB5_TGS_NAME_SIZE, KRB5_TGS_NAME},
    {0, 0, 0} };

krb5_data db_creator_entries[] = {
    {0, sizeof("db_creation")-1, "db_creation"} };

/* XXX knows about contents of krb5_principal, and that tgt names
   are of form TGT/REALM@REALM */
krb5_principal_data tgt_princ = {
    0,                                      /* magic number */
    {0, 0, 0},                              /* krb5_data realm */
    tgt_princ_entries,                      /* krb5_data *data */
    2,                                      /* int length */
    KRB5_NT_SRV_INST                        /* int type */
};

krb5_principal_data db_create_princ = {
    0,                                      /* magic number */
    {0, 0, 0},                              /* krb5_data realm */
    db_creator_entries,                     /* krb5_data *data */
    1,                                      /* int length */
    KRB5_NT_SRV_INST                        /* int type */
};

extern char *mkey_password;

extern char *progname;
extern int exit_status;
extern kadm5_config_params global_params;
extern krb5_context util_context;

void kdb5_create(argc, argv)
    int argc;
    char *argv[];
{
    int optchar;

    krb5_error_code retval;
    char *pw_str = 0;
    unsigned int pw_size = 0;
    int do_stash = 0;
    krb5_data pwd, seed;
    kdb_log_context *log_ctx;
    krb5_kvno mkey_kvno;

    while ((optchar = getopt(argc, argv, "sW")) != -1) {
        switch(optchar) {
        case 's':
            do_stash++;
            break;
        case 'W':
            /* Ignore (deprecated weak random option). */
            break;
        case '?':
        default:
            usage();
            return;
        }
    }

    rblock.max_life = global_params.max_life;
    rblock.max_rlife = global_params.max_rlife;
    rblock.expiration = global_params.expiration;
    rblock.flags = global_params.flags;
    rblock.nkslist = global_params.num_keysalts;
    rblock.kslist = global_params.keysalts;

    log_ctx = util_context->kdblog_context;

    /* assemble & parse the master key name */

    if ((retval = krb5_db_setup_mkey_name(util_context,
                                          global_params.mkey_name,
                                          global_params.realm,
                                          &mkey_fullname, &master_princ))) {
        com_err(progname, retval, _("while setting up master key name"));
        exit_status++; return;
    }

    krb5_princ_set_realm_data(util_context, &db_create_princ, global_params.realm);
    krb5_princ_set_realm_length(util_context, &db_create_princ, strlen(global_params.realm));
    krb5_princ_set_realm_data(util_context, &tgt_princ, global_params.realm);
    krb5_princ_set_realm_length(util_context, &tgt_princ, strlen(global_params.realm));
    krb5_princ_component(util_context, &tgt_princ,1)->data = global_params.realm;
    krb5_princ_component(util_context, &tgt_princ,1)->length = strlen(global_params.realm);

    printf(_("Initializing database '%s' for realm '%s',\n"
             "master key name '%s'\n"),
           global_params.dbname, global_params.realm, mkey_fullname);

    if (!mkey_password) {
        printf(_("You will be prompted for the database Master Password.\n"));
        printf(_("It is important that you NOT FORGET this password.\n"));
        fflush(stdout);

        pw_size = 1024;
        pw_str = malloc(pw_size);
        if (pw_str == NULL) {
            com_err(progname, ENOMEM, _("while creating new master key"));
            exit_status++; return;
        }

        retval = krb5_read_password(util_context, KRB5_KDC_MKEY_1, KRB5_KDC_MKEY_2,
                                    pw_str, &pw_size);
        if (retval) {
            com_err(progname, retval,
                    _("while reading master key from keyboard"));
            exit_status++; return;
        }
        mkey_password = pw_str;
    }

    pwd.data = mkey_password;
    pwd.length = strlen(mkey_password);
    retval = krb5_principal2salt(util_context, master_princ, &master_salt);
    if (retval) {
        com_err(progname, retval, _("while calculating master key salt"));
        exit_status++; return;
    }

    retval = krb5_c_string_to_key(util_context, master_keyblock.enctype,
                                  &pwd, &master_salt, &master_keyblock);
    if (retval) {
        com_err(progname, retval,
                _("while transforming master key from password"));
        exit_status++; return;
    }

    rblock.key = &master_keyblock;

    seed = make_data(master_keyblock.contents, master_keyblock.length);

    if ((retval = krb5_c_random_seed(util_context, &seed))) {
        com_err(progname, retval,
                _("while initializing random key generator"));
        exit_status++; return;
    }
    if ((retval = krb5_db_create(util_context,
                                 db5util_db_args))) {
        com_err(progname, retval, _("while creating database '%s'"),
                global_params.dbname);
        exit_status++; return;
    }
/*     if ((retval = krb5_db_fini(util_context))) { */
/*         com_err(progname, retval, "while closing current database"); */
/*         exit_status++; return; */
/*     } */
/*     if ((retval = krb5_db_open(util_context, db5util_db_args, KRB5_KDB_OPEN_RW))) { */
/*      com_err(progname, retval, "while initializing the database '%s'", */
/*              global_params.dbname); */
/*      exit_status++; return; */
/*     } */

    if (log_ctx && log_ctx->iproprole) {
        retval = ulog_map(util_context, global_params.iprop_logfile,
                          global_params.iprop_ulogsize);
        if (retval) {
            com_err(argv[0], retval, _("while creating update log"));
            exit_status++;
            return;
        }

        /*
         * We're reinitializing the update log in case one already
         * existed, but this should never happen.
         */
        retval = ulog_init_header(util_context);
        if (retval) {
            com_err(argv[0], retval, _("while initializing update log"));
            exit_status++;
            return;
        }

        /*
         * Since we're creating a new db we shouldn't worry about
         * adding the initial principals since any replica might as
         * well do full resyncs from this newly created db.
         */
        log_ctx->iproprole = IPROP_NULL;
    }

    if ((retval = add_principal(util_context, master_princ, MASTER_KEY, &rblock)) ||
        (retval = add_principal(util_context, &tgt_princ, TGT_KEY, &rblock))) {
        com_err(progname, retval, _("while adding entries to the database"));
        exit_status++; return;
    }



    /*
     * Always stash the master key so kadm5_create does not prompt for
     * it; delete the file below if it was not requested.  DO NOT EXIT
     * BEFORE DELETING THE KEYFILE if do_stash is not set.
     */

    /*
     * Determine the kvno to use, it must be that used to create the master key
     * princ.
     */
    if (global_params.mask & KADM5_CONFIG_KVNO)
        mkey_kvno = global_params.kvno; /* user specified */
    else
        mkey_kvno = 1;  /* Default */

    retval = krb5_db_store_master_key(util_context,
                                      global_params.stash_file,
                                      master_princ,
                                      mkey_kvno,
                                      &master_keyblock,
                                      mkey_password);
    if (retval) {
        com_err(progname, retval, _("while storing key"));
        printf(_("Warning: couldn't stash master key.\n"));
    }
    /* clean up */
    zapfree(pw_str, pw_size);
    free(master_salt.data);

    if (kadm5_create(&global_params)) {
        if (!do_stash) unlink(global_params.stash_file);
        exit_status++;
        return;
    }
    if (!do_stash) unlink(global_params.stash_file);

    return;
}

static krb5_error_code
tgt_keysalt_iterate(ksent, ptr)
    krb5_key_salt_tuple *ksent;
    krb5_pointer        ptr;
{
    krb5_context        context;
    krb5_error_code     kret;
    struct iterate_args *iargs;
    krb5_keyblock       key;
    krb5_int32          ind;
    krb5_data   pwd;

    iargs = (struct iterate_args *) ptr;
    kret = 0;

    context = iargs->ctx;

    /*
     * Convert the master key password into a key for this particular
     * encryption system.
     */
    pwd.data = mkey_password;
    pwd.length = strlen(mkey_password);
    kret = krb5_c_random_seed(context, &pwd);
    if (kret)
        return kret;

    if (!(kret = krb5_dbe_create_key_data(iargs->ctx, iargs->dbentp))) {
        ind = iargs->dbentp->n_key_data-1;
        if (!(kret = krb5_c_make_random_key(context, ksent->ks_enctype,
                                            &key))) {
            kret = krb5_dbe_encrypt_key_data(context, iargs->rblock->key,
                                             &key, NULL, 1,
                                             &iargs->dbentp->key_data[ind]);
            krb5_free_keyblock_contents(context, &key);
        }
    }

    return(kret);
}

static krb5_error_code
add_principal(context, princ, op, pblock)
    krb5_context context;
    krb5_principal princ;
    enum ap_op op;
    struct realm_info *pblock;
{
    krb5_error_code       retval;
    krb5_db_entry         *entry = NULL;
    krb5_kvno             mkey_kvno;
    krb5_timestamp        now;
    struct iterate_args   iargs;
    krb5_actkvno_node     actkvno;

    entry = calloc(1, sizeof(*entry));
    if (entry == NULL)
        return ENOMEM;

    entry->len = KRB5_KDB_V1_BASE_LENGTH;
    entry->attributes = pblock->flags;
    entry->max_life = pblock->max_life;
    entry->max_renewable_life = pblock->max_rlife;
    entry->expiration = pblock->expiration;

    if ((retval = krb5_copy_principal(context, princ, &entry->princ)))
        goto cleanup;

    if ((retval = krb5_timeofday(context, &now)))
        goto cleanup;

    if ((retval = krb5_dbe_update_mod_princ_data(context, entry,
                                                 now, &db_create_princ)))
        goto cleanup;

    switch (op) {
    case MASTER_KEY:
        if ((entry->key_data=(krb5_key_data*)malloc(sizeof(krb5_key_data)))
            == NULL)
            goto cleanup;
        memset(entry->key_data, 0, sizeof(krb5_key_data));
        entry->n_key_data = 1;

        if (global_params.mask & KADM5_CONFIG_KVNO)
            mkey_kvno = global_params.kvno; /* user specified */
        else
            mkey_kvno = 1;  /* Default */
        entry->attributes |= KRB5_KDB_DISALLOW_ALL_TIX;
        if ((retval = krb5_dbe_encrypt_key_data(context, pblock->key,
                                                &master_keyblock, NULL,
                                                mkey_kvno, entry->key_data)))
            goto cleanup;
        /*
         * There should always be at least one "active" mkey so creating the
         * KRB5_TL_ACTKVNO entry now so the initial mkey is active.
         */
        actkvno.next = NULL;
        actkvno.act_kvno = mkey_kvno;
        /* earliest possible time in case system clock is set back */
        actkvno.act_time = 0;
        if ((retval = krb5_dbe_update_actkvno(context, entry, &actkvno)))
            goto cleanup;

        /* so getprinc shows the right kvno */
        if ((retval = krb5_dbe_update_mkvno(context, entry, mkey_kvno)))
            goto cleanup;

        break;
    case TGT_KEY:
        iargs.ctx = context;
        iargs.rblock = pblock;
        iargs.dbentp = entry;
        /*
         * Iterate through the key/salt list, ignoring salt types.
         */
        if ((retval = krb5_keysalt_iterate(pblock->kslist,
                                           pblock->nkslist,
                                           1,
                                           tgt_keysalt_iterate,
                                           (krb5_pointer) &iargs)))
            goto cleanup;
        break;
    case NULL_KEY:
        retval = EOPNOTSUPP;
        goto cleanup;
    default:
        break;
    }

    entry->mask = (KADM5_KEY_DATA | KADM5_PRINCIPAL | KADM5_ATTRIBUTES |
                   KADM5_MAX_LIFE | KADM5_MAX_RLIFE | KADM5_TL_DATA |
                   KADM5_PRINC_EXPIRE_TIME);
    entry->attributes |= KRB5_KDB_LOCKDOWN_KEYS;

    retval = krb5_db_put_principal(context, entry);

cleanup:
    krb5_db_free_principal(context, entry);
    return retval;
}
