/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/ldap_util/kdb5_ldap_realm.c */
/*
 * Copyright 1990,1991,2001, 2002, 2008 by the Massachusetts Institute of Technology.
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

/* Copyright (c) 2004-2005, Novell, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *   * The copyright holder's name is not used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Create / Modify / Destroy / View / List realm(s)
 */

#include <k5-int.h>
#include <kadm5/admin.h>
#include <adm_proto.h>
#include "kdb5_ldap_util.h"
#include "kdb5_ldap_list.h"
#include <ldap_principal.h>
#include <ldap_krbcontainer.h>
extern time_t get_date(char *); /* kadmin/cli/getdate.o */

char *yes = "yes\n"; /* \n to compare against result of fgets */

krb5_data tgt_princ_entries[] = {
    {0, KRB5_TGS_NAME_SIZE, KRB5_TGS_NAME},
    {0, 0, 0} };

krb5_data db_creator_entries[] = {
    {0, sizeof("db_creation")-1, "db_creation"} };


static krb5_principal_data db_create_princ = {
    0,                                  /* magic number */
    {0, 0, 0},                          /* krb5_data realm */
    db_creator_entries,                 /* krb5_data *data */
    1,                                  /* int length */
    KRB5_NT_SRV_INST                    /* int type */
};

extern char *mkey_password;
extern char *progname;
extern kadm5_config_params global_params;

static void print_realm_params(krb5_ldap_realm_params *rparams, int mask);
static int kdb_ldap_create_principal (krb5_context context, krb5_principal
                                      princ, enum ap_op op,
                                      struct realm_info *pblock,
                                      const krb5_keyblock *master_keyblock);


static char *strdur(time_t duration);
static int get_ticket_policy(krb5_ldap_realm_params *rparams, int *i, char *argv[],int argc);
static krb5_error_code krb5_dbe_update_mod_princ_data_new (krb5_context context, krb5_db_entry *entry, krb5_timestamp mod_date, krb5_const_principal mod_princ);
static krb5_error_code krb5_dbe_update_tl_data_new ( krb5_context context, krb5_db_entry *entry, krb5_tl_data *new_tl_data);

#define ADMIN_LIFETIME 60*60*3 /* 3 hours */
#define CHANGEPW_LIFETIME 60*5 /* 5 minutes */

static int
get_ticket_policy(krb5_ldap_realm_params *rparams, int *i, char *argv[],
                  int argc)
{
    time_t date;
    time_t now;
    int mask = 0;
    krb5_error_code retval = 0;
    char *me = progname;

    time(&now);
    if (!strcmp(argv[*i], "-maxtktlife")) {
        if (++(*i) > argc-1)
            return 0;
        date = get_date(argv[*i]);
        if (date == (time_t)(-1)) {
            retval = EINVAL;
            com_err(me, retval, _("while providing time specification"));
            return 0;
        }
        rparams->max_life = date-now;
        mask |= LDAP_REALM_MAXTICKETLIFE;
    }


    else if (!strcmp(argv[*i], "-maxrenewlife")) {
        if (++(*i) > argc-1)
            return 0;

        date = get_date(argv[*i]);
        if (date == (time_t)(-1)) {
            retval = EINVAL;
            com_err(me, retval, _("while providing time specification"));
            return 0;
        }
        rparams->max_renewable_life = date-now;
        mask |= LDAP_REALM_MAXRENEWLIFE;
    } else if (!strcmp((argv[*i] + 1), "allow_postdated")) {
        if (*(argv[*i]) == '+')
            rparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_POSTDATED);
        else if (*(argv[*i]) == '-')
            rparams->tktflags |= KRB5_KDB_DISALLOW_POSTDATED;
        else
            return 0;

        mask |= LDAP_REALM_KRBTICKETFLAGS;
    } else if (!strcmp((argv[*i] + 1), "allow_forwardable")) {
        if (*(argv[*i]) == '+')
            rparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_FORWARDABLE);

        else if (*(argv[*i]) == '-')
            rparams->tktflags |= KRB5_KDB_DISALLOW_FORWARDABLE;
        else
            return 0;

        mask |= LDAP_REALM_KRBTICKETFLAGS;
    } else if (!strcmp((argv[*i] + 1), "allow_renewable")) {
        if (*(argv[*i]) == '+')
            rparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_RENEWABLE);
        else if (*(argv[*i]) == '-')
            rparams->tktflags |= KRB5_KDB_DISALLOW_RENEWABLE;
        else
            return 0;

        mask |= LDAP_REALM_KRBTICKETFLAGS;
    } else if (!strcmp((argv[*i] + 1), "allow_proxiable")) {
        if (*(argv[*i]) == '+')
            rparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_PROXIABLE);
        else if (*(argv[*i]) == '-')
            rparams->tktflags |= KRB5_KDB_DISALLOW_PROXIABLE;
        else
            return 0;

        mask |= LDAP_REALM_KRBTICKETFLAGS;
    } else if (!strcmp((argv[*i] + 1), "allow_dup_skey")) {
        if (*(argv[*i]) == '+')
            rparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_DUP_SKEY);
        else if (*(argv[*i]) == '-')
            rparams->tktflags |= KRB5_KDB_DISALLOW_DUP_SKEY;
        else
            return 0;

        mask |= LDAP_REALM_KRBTICKETFLAGS;
    }

    else if (!strcmp((argv[*i] + 1), "requires_preauth")) {
        if (*(argv[*i]) == '+')
            rparams->tktflags |= KRB5_KDB_REQUIRES_PRE_AUTH;
        else if (*(argv[*i]) == '-')
            rparams->tktflags &= (int)(~KRB5_KDB_REQUIRES_PRE_AUTH);
        else
            return 0;

        mask |= LDAP_REALM_KRBTICKETFLAGS;
    } else if (!strcmp((argv[*i] + 1), "requires_hwauth")) {
        if (*(argv[*i]) == '+')
            rparams->tktflags |= KRB5_KDB_REQUIRES_HW_AUTH;
        else if (*(argv[*i]) == '-')
            rparams->tktflags &= (int)(~KRB5_KDB_REQUIRES_HW_AUTH);
        else
            return 0;

        mask |= LDAP_REALM_KRBTICKETFLAGS;
    } else if (!strcmp((argv[*i] + 1), "allow_svr")) {
        if (*(argv[*i]) == '+')
            rparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_SVR);
        else if (*(argv[*i]) == '-')
            rparams->tktflags |= KRB5_KDB_DISALLOW_SVR;
        else
            return 0;

        mask |= LDAP_REALM_KRBTICKETFLAGS;
    } else if (!strcmp((argv[*i] + 1), "allow_tgs_req")) {
        if (*(argv[*i]) == '+')
            rparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_TGT_BASED);
        else if (*(argv[*i]) == '-')
            rparams->tktflags |= KRB5_KDB_DISALLOW_TGT_BASED;
        else
            return 0;

        mask |= LDAP_REALM_KRBTICKETFLAGS;
    } else if (!strcmp((argv[*i] + 1), "allow_tix")) {
        if (*(argv[*i]) == '+')
            rparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_ALL_TIX);
        else if (*(argv[*i]) == '-')
            rparams->tktflags |= KRB5_KDB_DISALLOW_ALL_TIX;
        else
            return 0;

        mask |= LDAP_REALM_KRBTICKETFLAGS;
    } else if (!strcmp((argv[*i] + 1), "needchange")) {
        if (*(argv[*i]) == '+')
            rparams->tktflags |= KRB5_KDB_REQUIRES_PWCHANGE;
        else if (*(argv[*i]) == '-')
            rparams->tktflags &= (int)(~KRB5_KDB_REQUIRES_PWCHANGE);
        else
            return 0;

        mask |= LDAP_REALM_KRBTICKETFLAGS;
    } else if (!strcmp((argv[*i] + 1), "password_changing_service")) {
        if (*(argv[*i]) == '+')
            rparams->tktflags |= KRB5_KDB_PWCHANGE_SERVICE;
        else if (*(argv[*i]) == '-')
            rparams->tktflags &= (int)(~KRB5_KDB_PWCHANGE_SERVICE);
        else
            return 0;

        mask |=LDAP_REALM_KRBTICKETFLAGS;
    }

    return mask;
}

/* Create a special principal using two specified components. */
static krb5_error_code
create_fixed_special(krb5_context context, struct realm_info *rinfo,
                     krb5_keyblock *mkey, const char *comp1, const char *comp2)
{
    krb5_error_code ret;
    krb5_principal princ;
    const char *realm = global_params.realm;

    ret = krb5_build_principal(context, &princ, strlen(realm), realm, comp1,
                               comp2, (const char *)NULL);
    if (ret)
        return ret;
    ret = kdb_ldap_create_principal(context, princ, TGT_KEY, rinfo, mkey);
    krb5_free_principal(context, princ);
    return ret;

}

/* Create all special principals for the realm. */
static krb5_error_code
create_special_princs(krb5_context context, krb5_principal master_princ,
                      krb5_keyblock *mkey)
{
    krb5_error_code ret;
    struct realm_info rblock;

    rblock.max_life = global_params.max_life;
    rblock.max_rlife = global_params.max_rlife;
    rblock.expiration = global_params.expiration;
    rblock.flags = global_params.flags;
    rblock.key = mkey;
    rblock.nkslist = global_params.num_keysalts;
    rblock.kslist = global_params.keysalts;

    /* Create master principal. */
    rblock.flags |= KRB5_KDB_DISALLOW_ALL_TIX;
    ret = kdb_ldap_create_principal(context, master_princ, MASTER_KEY, &rblock,
                                    mkey);
    if (ret)
        return ret;

    /* Create local krbtgt principal. */
    rblock.flags = 0;
    ret = create_fixed_special(context, &rblock, mkey, KRB5_TGS_NAME,
                               global_params.realm);
    if (ret)
        return ret;

    /* Create kadmin/admin. */
    rblock.max_life = ADMIN_LIFETIME;
    rblock.flags = KRB5_KDB_DISALLOW_TGT_BASED;
    ret = create_fixed_special(context, &rblock, mkey, "kadmin", "admin");
    if (ret)
        return ret;

    /* Create kadmin/changepw. */
    rblock.max_life = CHANGEPW_LIFETIME;
    rblock.flags = KRB5_KDB_DISALLOW_TGT_BASED | KRB5_KDB_PWCHANGE_SERVICE;
    ret = create_fixed_special(context, &rblock, mkey, "kadmin", "changepw");
    if (ret)
        return ret;

    /* Create kadmin/history. */
    rblock.max_life = global_params.max_life;
    rblock.flags = 0;
    return create_fixed_special(context, &rblock, mkey, "kadmin", "history");
}

/*
 * This function will create a realm on the LDAP Server, with
 * the specified attributes.
 */
void
kdb5_ldap_create(int argc, char *argv[])
{
    krb5_error_code retval = 0;
    krb5_keyblock master_keyblock;
    krb5_ldap_realm_params *rparams = NULL;
    krb5_principal master_princ = NULL;
    kdb5_dal_handle *dal_handle = NULL;
    krb5_ldap_context *ldap_context=NULL;
    krb5_boolean realm_obj_created = FALSE;
    krb5_boolean create_complete = FALSE;
    krb5_boolean print_usage = FALSE;
    krb5_boolean no_msg = FALSE;
    char *oldcontainerref=NULL;
    char pw_str[1024];
    int do_stash = 0;
    int i = 0;
    int mask = 0, ret_mask = 0;
    char **list = NULL;

    memset(&master_keyblock, 0, sizeof(master_keyblock));

    rparams = (krb5_ldap_realm_params *)malloc(
        sizeof(krb5_ldap_realm_params));
    if (rparams == NULL) {
        retval = ENOMEM;
        goto cleanup;
    }
    memset(rparams, 0, sizeof(krb5_ldap_realm_params));

    /* Parse the arguments */
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-subtrees")) {
            if (++i > argc-1)
                goto err_usage;

            if (strncmp(argv[i], "", strlen(argv[i]))!=0) {
                list = (char **) calloc(MAX_LIST_ENTRIES, sizeof(char *));
                if (list == NULL) {
                    retval = ENOMEM;
                    goto cleanup;
                }
                if ((retval = krb5_parse_list(argv[i], LIST_DELIMITER, list))) {
                    free(list);
                    list = NULL;
                    goto cleanup;
                }

                rparams->subtreecount=0;
                while (list[rparams->subtreecount]!=NULL)
                    (rparams->subtreecount)++;
                rparams->subtree = list;
            } else if (strncmp(argv[i], "", strlen(argv[i]))==0) {
                /* dont allow subtree value to be set at the root(NULL, "") of the tree */
                com_err(progname, EINVAL,
                        _("for subtree while creating realm '%s'"),
                        global_params.realm);
                goto err_nomsg;
            }
            rparams->subtree[rparams->subtreecount] = NULL;
            mask |= LDAP_REALM_SUBTREE;
        } else if (!strcmp(argv[i], "-containerref")) {
            if (++i > argc-1)
                goto err_usage;
            if (strncmp(argv[i], "", strlen(argv[i]))==0) {
                /* dont allow containerref value to be set at the root(NULL, "") of the tree */
                com_err(progname, EINVAL,
                        _("for container reference while creating realm '%s'"),
                        global_params.realm);
                goto err_nomsg;
            }
            free(rparams->containerref);
            rparams->containerref = strdup(argv[i]);
            if (rparams->containerref == NULL) {
                retval = ENOMEM;
                goto cleanup;
            }
            mask |= LDAP_REALM_CONTREF;
        } else if (!strcmp(argv[i], "-sscope")) {
            if (++i > argc-1)
                goto err_usage;
            /* Possible values for search scope are
             * one (or 1) and sub (or 2)
             */
            if (!strcasecmp(argv[i], "one")) {
                rparams->search_scope = 1;
            } else if (!strcasecmp(argv[i], "sub")) {
                rparams->search_scope = 2;
            } else {
                rparams->search_scope = atoi(argv[i]);
                if ((rparams->search_scope != 1) &&
                    (rparams->search_scope != 2)) {
                    com_err(progname, EINVAL, _("invalid search scope while "
                                                "creating realm '%s'"),
                            global_params.realm);
                    goto err_nomsg;
                }
            }
            mask |= LDAP_REALM_SEARCHSCOPE;
        }
        else if (!strcmp(argv[i], "-s")) {
            do_stash = 1;
        } else if ((ret_mask= get_ticket_policy(rparams,&i,argv,argc)) !=0) {
            mask|=ret_mask;
        }

        else {
            printf(_("'%s' is an invalid option\n"), argv[i]);
            goto err_usage;
        }
    }

    krb5_princ_set_realm_data(util_context, &db_create_princ, global_params.realm);
    krb5_princ_set_realm_length(util_context, &db_create_princ, strlen(global_params.realm));

    printf(_("Initializing database for realm '%s'\n"), global_params.realm);

    if (!mkey_password) {
        unsigned int pw_size;
        printf(_("You will be prompted for the database Master Password.\n"));
        printf(_("It is important that you NOT FORGET this password.\n"));
        fflush(stdout);

        pw_size = sizeof (pw_str);
        memset(pw_str, 0, pw_size);

        retval = krb5_read_password(util_context, KRB5_KDC_MKEY_1, KRB5_KDC_MKEY_2,
                                    pw_str, &pw_size);
        if (retval) {
            com_err(progname, retval,
                    _("while reading master key from keyboard"));
            goto err_nomsg;
        }
        mkey_password = pw_str;
    }

    rparams->realm_name = strdup(global_params.realm);
    if (rparams->realm_name == NULL) {
        retval = ENOMEM;
        com_err(progname, ENOMEM, _("while creating realm '%s'"),
                global_params.realm);
        goto err_nomsg;
    }

    dal_handle = util_context->dal_handle;
    ldap_context = (krb5_ldap_context *) dal_handle->db_context;
    if (!ldap_context) {
        retval = EINVAL;
        goto cleanup;
    }

    /* read the kerberos container */
    retval = krb5_ldap_read_krbcontainer_dn(util_context,
                                            &ldap_context->container_dn);
    if (retval) {
        /* Prompt the user for entering the DN of Kerberos container */
        char krb_location[MAX_KRB_CONTAINER_LEN];
        int krb_location_len = 0;

        printf(_("Enter DN of Kerberos container: "));
        if (fgets(krb_location, MAX_KRB_CONTAINER_LEN, stdin) != NULL) {
            /* Remove the newline character at the end */
            krb_location_len = strlen(krb_location);
            if ((krb_location[krb_location_len - 1] == '\n') ||
                (krb_location[krb_location_len - 1] == '\r')) {
                krb_location[krb_location_len - 1] = '\0';
                krb_location_len--;
            }
            ldap_context->container_dn = strdup(krb_location);
            if (ldap_context->container_dn == NULL) {
                retval = ENOMEM;
                goto cleanup;
            }
        }
    }

    /* create the kerberos container if it doesn't exist */
    retval = krb5_ldap_create_krbcontainer(util_context,
                                           ldap_context->container_dn);
    if (retval)
        goto cleanup;

    if ((retval = krb5_ldap_create_realm(util_context,
                                         /* global_params.realm, */ rparams, mask))) {
        goto cleanup;
    }

    /* We just created the Realm container. Here starts our transaction tracking */
    realm_obj_created = TRUE;

    if ((retval = krb5_ldap_read_realm_params(util_context,
                                              global_params.realm,
                                              &(ldap_context->lrparams),
                                              &mask))) {
        com_err(progname, retval, _("while reading information of realm '%s'"),
                global_params.realm);
        goto err_nomsg;
    }
    free(ldap_context->lrparams->realm_name);
    ldap_context->lrparams->realm_name = strdup(global_params.realm);
    if (ldap_context->lrparams->realm_name == NULL) {
        retval = ENOMEM;
        goto cleanup;
    }

    /* assemble & parse the master key name */
    if ((retval = krb5_db_setup_mkey_name(util_context,
                                          global_params.mkey_name,
                                          global_params.realm,
                                          0, &master_princ))) {
        com_err(progname, retval, _("while setting up master key name"));
        goto err_nomsg;
    }

    /* Obtain master key from master password */
    {
        krb5_data master_salt, pwd;

        pwd.data = mkey_password;
        pwd.length = strlen(mkey_password);
        retval = krb5_principal2salt(util_context, master_princ, &master_salt);
        if (retval) {
            com_err(progname, retval, _("while calculating master key salt"));
            goto err_nomsg;
        }

        retval = krb5_c_string_to_key(util_context, global_params.enctype,
                                      &pwd, &master_salt, &master_keyblock);

        if (master_salt.data)
            free(master_salt.data);

        if (retval) {
            com_err(progname, retval,
                    _("while transforming master key from password"));
            goto err_nomsg;
        }
    }

    /* Create special principals (not in the container reference). */
    oldcontainerref = ldap_context->lrparams->containerref;
    ldap_context->lrparams->containerref = NULL;
    retval = create_special_princs(util_context, master_princ,
                                   &master_keyblock);
    ldap_context->lrparams->containerref = oldcontainerref;
    if (retval) {
        com_err(progname, retval, _("while adding entries to the database"));
        goto err_nomsg;
    }

    /* The Realm creation is completed. Here is the end of transaction */
    create_complete = TRUE;

    /* Stash the master key only if '-s' option is specified */
    if (do_stash || global_params.mask & KADM5_CONFIG_STASH_FILE) {
        krb5_kvno mkey_kvno;
        /*
         * Determine the kvno to use, it must be that used to create the master
         * key princ.
         */
        if (global_params.mask & KADM5_CONFIG_KVNO)
            mkey_kvno = global_params.kvno; /* user specified */
        else
            mkey_kvno = 1;  /* Default */

        retval = krb5_db_store_master_key(util_context,
                                          global_params.stash_file,
                                          master_princ,
                                          mkey_kvno,
                                          &master_keyblock, NULL);
        if (retval) {
            com_err(progname, errno, _("while storing key"));
            printf(_("Warning: couldn't stash master key.\n"));
        }
    }

    goto cleanup;


err_usage:
    print_usage = TRUE;

err_nomsg:
    no_msg = TRUE;

cleanup:
    /* If the Realm creation is not complete, do the roll-back here */
    if ((realm_obj_created) && (!create_complete))
        krb5_ldap_delete_realm(util_context, global_params.realm);

    if (rparams)
        krb5_ldap_free_realm_params(rparams);

    memset (pw_str, 0, sizeof (pw_str));

    if (print_usage)
        db_usage(CREATE_REALM);

    if (retval) {
        if (!no_msg) {
            com_err(progname, retval, _("while creating realm '%s'"),
                    global_params.realm);
        }
        exit_status++;
    }

    krb5_free_keyblock_contents(util_context, &master_keyblock);
    krb5_free_principal(util_context, master_princ);
}


/*
 * This function will modify the attributes of a given realm object
 */
void
kdb5_ldap_modify(int argc, char *argv[])
{
    krb5_error_code retval = 0;
    krb5_ldap_realm_params *rparams = NULL;
    krb5_boolean print_usage = FALSE;
    krb5_boolean no_msg = FALSE;
    kdb5_dal_handle *dal_handle = NULL;
    krb5_ldap_context *ldap_context=NULL;
    int i = 0;
    int mask = 0, rmask = 0, ret_mask = 0;
    char **slist = {NULL};

    dal_handle = util_context->dal_handle;
    ldap_context = (krb5_ldap_context *) dal_handle->db_context;
    if (!(ldap_context)) {
        retval = EINVAL;
        goto cleanup;
    }

    retval = krb5_ldap_read_krbcontainer_dn(util_context,
                                            &ldap_context->container_dn);
    if (retval) {
        com_err(progname, retval,
                _("while reading Kerberos container information"));
        goto err_nomsg;
    }

    retval = krb5_ldap_read_realm_params(util_context,
                                         global_params.realm, &rparams, &rmask);
    if (retval)
        goto cleanup;
    /* Parse the arguments */
    for (i = 1; i < argc; i++) {
        int k = 0;
        if (!strcmp(argv[i], "-subtrees")) {
            if (++i > argc-1)
                goto err_usage;

            if (rmask & LDAP_REALM_SUBTREE) {
                if (rparams->subtree) {
                    for (k=0; k<rparams->subtreecount && rparams->subtree[k]; k++)
                        free(rparams->subtree[k]);
                    free(rparams->subtree);
                    rparams->subtreecount=0;
                    rparams->subtree = NULL;
                }
            }
            if (strncmp(argv[i] ,"", strlen(argv[i]))!=0) {
                slist =  (char **) calloc(MAX_LIST_ENTRIES, sizeof(char *));
                if (slist == NULL) {
                    retval = ENOMEM;
                    goto cleanup;
                }
                if ((retval = krb5_parse_list(argv[i], LIST_DELIMITER, slist))) {
                    free(slist);
                    slist = NULL;
                    goto cleanup;
                }

                rparams->subtreecount=0;
                while (slist[rparams->subtreecount]!=NULL)
                    (rparams->subtreecount)++;
                rparams->subtree =  slist;
            } else if (strncmp(argv[i], "", strlen(argv[i]))==0) {
                /* dont allow subtree value to be set at the root(NULL, "") of the tree */
                com_err(progname, EINVAL,
                        _("for subtree while modifying realm '%s'"),
                        global_params.realm);
                goto err_nomsg;
            }
            rparams->subtree[rparams->subtreecount] = NULL;
            mask |= LDAP_REALM_SUBTREE;
        } else if (!strncmp(argv[i], "-containerref", strlen(argv[i]))) {
            if (++i > argc-1)
                goto err_usage;
            if (strncmp(argv[i], "", strlen(argv[i]))==0) {
                /* dont allow containerref value to be set at the root(NULL, "") of the tree */
                com_err(progname, EINVAL, _("for container reference while "
                                            "modifying realm '%s'"),
                        global_params.realm);
                goto err_nomsg;
            }
            free(rparams->containerref);
            rparams->containerref = strdup(argv[i]);
            if (rparams->containerref == NULL) {
                retval = ENOMEM;
                goto cleanup;
            }
            mask |= LDAP_REALM_CONTREF;
        } else if (!strcmp(argv[i], "-sscope")) {
            if (++i > argc-1)
                goto err_usage;
            /* Possible values for search scope are
             * one (or 1) and sub (or 2)
             */
            if (strcasecmp(argv[i], "one") == 0) {
                rparams->search_scope = 1;
            } else if (strcasecmp(argv[i], "sub") == 0) {
                rparams->search_scope = 2;
            } else {
                rparams->search_scope = atoi(argv[i]);
                if ((rparams->search_scope != 1) &&
                    (rparams->search_scope != 2)) {
                    retval = EINVAL;
                    com_err(progname, retval,
                            _("specified for search scope while modifying "
                              "information of realm '%s'"),
                            global_params.realm);
                    goto err_nomsg;
                }
            }
            mask |= LDAP_REALM_SEARCHSCOPE;
        }
        else if ((ret_mask= get_ticket_policy(rparams,&i,argv,argc)) !=0) {
            mask|=ret_mask;
        } else {
            printf(_("'%s' is an invalid option\n"), argv[i]);
            goto err_usage;
        }
    }

    if ((retval = krb5_ldap_modify_realm(util_context,
                                         /* global_params.realm, */ rparams, mask))) {
        goto cleanup;
    }

    goto cleanup;

err_usage:
    print_usage = TRUE;

err_nomsg:
    no_msg = TRUE;

cleanup:
    krb5_ldap_free_realm_params(rparams);

    if (print_usage) {
        db_usage(MODIFY_REALM);
    }

    if (retval) {
        if (!no_msg)
            com_err(progname, retval,
                    _("while modifying information of realm '%s'"),
                    global_params.realm);
        exit_status++;
    }

    return;
}



/*
 * This function displays the attributes of a Realm
 */
void
kdb5_ldap_view(int argc, char *argv[])
{
    krb5_ldap_realm_params *rparams = NULL;
    krb5_error_code retval = 0;
    kdb5_dal_handle *dal_handle=NULL;
    krb5_ldap_context *ldap_context=NULL;
    int mask = 0;

    dal_handle = util_context->dal_handle;
    ldap_context = (krb5_ldap_context *) dal_handle->db_context;
    if (!(ldap_context)) {
        retval = EINVAL;
        com_err(progname, retval, _("while initializing database"));
        exit_status++;
        return;
    }

    /* Read the kerberos container information */
    retval = krb5_ldap_read_krbcontainer_dn(util_context,
                                            &ldap_context->container_dn);
    if (retval) {
        com_err(progname, retval,
                _("while reading kerberos container information"));
        exit_status++;
        return;
    }

    if ((retval = krb5_ldap_read_realm_params(util_context,
                                              global_params.realm, &rparams, &mask)) || (!rparams)) {
        com_err(progname, retval, _("while reading information of realm '%s'"),
                global_params.realm);
        exit_status++;
        return;
    }
    print_realm_params(rparams, mask);
    krb5_ldap_free_realm_params(rparams);

    return;
}

static char *
strdur(time_t duration)
{
    static char out[50];
    int neg, days, hours, minutes, seconds;

    if (duration < 0) {
        duration *= -1;
        neg = 1;
    } else
        neg = 0;
    days = duration / (24 * 3600);
    duration %= 24 * 3600;
    hours = duration / 3600;
    duration %= 3600;
    minutes = duration / 60;
    duration %= 60;
    seconds = duration;
    snprintf(out, sizeof(out), "%s%d %s %02d:%02d:%02d", neg ? "-" : "",
             days, days == 1 ? "day" : "days",
             hours, minutes, seconds);
    return out;
}

/*
 * This function prints the attributes of a given realm to the
 * standard output.
 */
static void
print_realm_params(krb5_ldap_realm_params *rparams, int mask)
{
    char **slist = NULL;
    unsigned int num_entry_printed = 0, i = 0;

    /* Print the Realm Attributes on the standard output */
    printf("%25s: %-50s\n", _("Realm Name"), global_params.realm);
    if (mask & LDAP_REALM_SUBTREE) {
        for (i=0; rparams->subtree[i]!=NULL; i++)
            printf("%25s: %-50s\n", _("Subtree"), rparams->subtree[i]);
    }
    if (mask & LDAP_REALM_CONTREF)
        printf("%25s: %-50s\n", _("Principal Container Reference"),
               rparams->containerref);
    if (mask & LDAP_REALM_SEARCHSCOPE) {
        if ((rparams->search_scope != 1) &&
            (rparams->search_scope != 2)) {
            printf("%25s: %-50s\n", _("SearchScope"), _("Invalid !"));
        } else {
            printf("%25s: %-50s\n", _("SearchScope"),
                   (rparams->search_scope == 1) ? "ONE" : "SUB");
        }
    }
    if (mask & LDAP_REALM_KDCSERVERS) {
        printf("%25s:", _("KDC Services"));
        if (rparams->kdcservers != NULL) {
            num_entry_printed = 0;
            for (slist = rparams->kdcservers; *slist != NULL; slist++) {
                if (num_entry_printed)
                    printf(" %25s %-50s\n", " ", *slist);
                else
                    printf(" %-50s\n", *slist);
                num_entry_printed++;
            }
        }
        if (num_entry_printed == 0)
            printf("\n");
    }
    if (mask & LDAP_REALM_ADMINSERVERS) {
        printf("%25s:", _("Admin Services"));
        if (rparams->adminservers != NULL) {
            num_entry_printed = 0;
            for (slist = rparams->adminservers; *slist != NULL; slist++) {
                if (num_entry_printed)
                    printf(" %25s %-50s\n", " ", *slist);
                else
                    printf(" %-50s\n", *slist);
                num_entry_printed++;
            }
        }
        if (num_entry_printed == 0)
            printf("\n");
    }
    if (mask & LDAP_REALM_PASSWDSERVERS) {
        printf("%25s:", _("Passwd Services"));
        if (rparams->passwdservers != NULL) {
            num_entry_printed = 0;
            for (slist = rparams->passwdservers; *slist != NULL; slist++) {
                if (num_entry_printed)
                    printf(" %25s %-50s\n", " ", *slist);
                else
                    printf(" %-50s\n", *slist);
                num_entry_printed++;
            }
        }
        if (num_entry_printed == 0)
            printf("\n");
    }

    if (mask & LDAP_REALM_MAXTICKETLIFE) {
        printf("%25s:", _("Maximum Ticket Life"));
        printf(" %s \n", strdur(rparams->max_life));
    }

    if (mask & LDAP_REALM_MAXRENEWLIFE) {
        printf("%25s:", _("Maximum Renewable Life"));
        printf(" %s \n", strdur(rparams->max_renewable_life));
    }

    if (mask & LDAP_REALM_KRBTICKETFLAGS) {
        int ticketflags = rparams->tktflags;

        printf("%25s: ", _("Ticket flags"));
        if (ticketflags & KRB5_KDB_DISALLOW_POSTDATED)
            printf("%s ","DISALLOW_POSTDATED");

        if (ticketflags & KRB5_KDB_DISALLOW_FORWARDABLE)
            printf("%s ","DISALLOW_FORWARDABLE");

        if (ticketflags & KRB5_KDB_DISALLOW_RENEWABLE)
            printf("%s ","DISALLOW_RENEWABLE");

        if (ticketflags & KRB5_KDB_DISALLOW_PROXIABLE)
            printf("%s ","DISALLOW_PROXIABLE");

        if (ticketflags & KRB5_KDB_DISALLOW_DUP_SKEY)
            printf("%s ","DISALLOW_DUP_SKEY");

        if (ticketflags & KRB5_KDB_REQUIRES_PRE_AUTH)
            printf("%s ","REQUIRES_PRE_AUTH");

        if (ticketflags & KRB5_KDB_REQUIRES_HW_AUTH)
            printf("%s ","REQUIRES_HW_AUTH");

        if (ticketflags & KRB5_KDB_DISALLOW_SVR)
            printf("%s ","DISALLOW_SVR");

        if (ticketflags & KRB5_KDB_DISALLOW_TGT_BASED)
            printf("%s ","DISALLOW_TGT_BASED");

        if (ticketflags & KRB5_KDB_DISALLOW_ALL_TIX)
            printf("%s ","DISALLOW_ALL_TIX");

        if (ticketflags & KRB5_KDB_REQUIRES_PWCHANGE)
            printf("%s ","REQUIRES_PWCHANGE");

        if (ticketflags & KRB5_KDB_PWCHANGE_SERVICE)
            printf("%s ","PWCHANGE_SERVICE");

        printf("\n");
    }


    return;
}



/*
 * This function lists the Realm(s) present under the Kerberos container
 * on the LDAP Server.
 */
void
kdb5_ldap_list(int argc, char *argv[])
{
    char **list = NULL;
    char **plist = NULL;
    krb5_error_code retval = 0;
    kdb5_dal_handle *dal_handle=NULL;
    krb5_ldap_context *ldap_context=NULL;

    dal_handle = util_context->dal_handle;
    ldap_context = (krb5_ldap_context *) dal_handle->db_context;
    if (!(ldap_context)) {
        retval = EINVAL;
        exit_status++;
        return;
    }

    /* Read the kerberos container information */
    retval = krb5_ldap_read_krbcontainer_dn(util_context,
                                            &ldap_context->container_dn);
    if (retval) {
        com_err(progname, retval,
                _("while reading kerberos container information"));
        exit_status++;
        return;
    }

    retval = krb5_ldap_list_realm(util_context, &list);
    if (retval != 0) {
        com_err(progname, retval, _("while listing realms"));
        exit_status++;
        return;
    }
    /* This is to handle the case of realm not present */
    if (list == NULL)
        return;

    for (plist = list; *plist != NULL; plist++) {
        printf("%s\n", *plist);
    }
    krb5_free_list_entries(list);
    free(list);

    return;
}

/*
 * Duplicating the following two functions here because
 * 'krb5_dbe_update_tl_data' uses backend specific memory allocation. The catch
 * here is that the backend is not initialized - kdb5_ldap_util doesn't go
 * through DAL.
 * 1. krb5_dbe_update_tl_data
 * 2. krb5_dbe_update_mod_princ_data
 */

/* Start duplicate code ... */

static krb5_error_code
krb5_dbe_update_tl_data_new(krb5_context context, krb5_db_entry *entry,
                            krb5_tl_data *new_tl_data)
{
    krb5_tl_data *tl_data = NULL;
    krb5_octet *tmp;

    /* copy the new data first, so we can fail cleanly if malloc()
     * fails */
    if ((tmp = (krb5_octet *) malloc (new_tl_data->tl_data_length)) == NULL)
        return (ENOMEM);

    /* Find an existing entry of the specified type and point at
     * it, or NULL if not found */

    if (new_tl_data->tl_data_type != KRB5_TL_DB_ARGS) { /* db_args can be multiple */
        for (tl_data = entry->tl_data; tl_data;
             tl_data = tl_data->tl_data_next)
            if (tl_data->tl_data_type == new_tl_data->tl_data_type)
                break;
    }

    /* if necessary, chain a new record in the beginning and point at it */

    if (!tl_data) {
        if ((tl_data = (krb5_tl_data *) malloc (sizeof(krb5_tl_data))) == NULL) {
            free(tmp);
            return (ENOMEM);
        }
        memset(tl_data, 0, sizeof(krb5_tl_data));
        tl_data->tl_data_next = entry->tl_data;
        entry->tl_data = tl_data;
        entry->n_tl_data++;
    }

    /* fill in the record */

    free(tl_data->tl_data_contents);

    tl_data->tl_data_type = new_tl_data->tl_data_type;
    tl_data->tl_data_length = new_tl_data->tl_data_length;
    tl_data->tl_data_contents = tmp;
    memcpy(tmp, new_tl_data->tl_data_contents, tl_data->tl_data_length);

    return (0);
}

static krb5_error_code
krb5_dbe_update_mod_princ_data_new(krb5_context context, krb5_db_entry *entry,
                                   krb5_timestamp mod_date,
                                   krb5_const_principal mod_princ)
{
    krb5_tl_data          tl_data;

    krb5_error_code       retval = 0;
    krb5_octet          * nextloc = 0;
    char                * unparse_mod_princ = 0;
    unsigned int        unparse_mod_princ_size;

    if ((retval = krb5_unparse_name(context, mod_princ,
                                    &unparse_mod_princ)))
        return(retval);

    unparse_mod_princ_size = strlen(unparse_mod_princ) + 1;

    if ((nextloc = (krb5_octet *) malloc(unparse_mod_princ_size + 4))
        == NULL) {
        free(unparse_mod_princ);
        return(ENOMEM);
    }

    tl_data.tl_data_type = KRB5_TL_MOD_PRINC;
    tl_data.tl_data_length = unparse_mod_princ_size + 4;
    tl_data.tl_data_contents = nextloc;

    /* Mod Date */
    krb5_kdb_encode_int32(mod_date, nextloc);

    /* Mod Princ */
    memcpy(nextloc+4, unparse_mod_princ, unparse_mod_princ_size);

    retval = krb5_dbe_update_tl_data_new(context, entry, &tl_data);

    free(unparse_mod_princ);
    free(nextloc);

    return(retval);
}

static krb5_error_code
kdb_ldap_tgt_keysalt_iterate(krb5_key_salt_tuple *ksent, krb5_pointer ptr)
{
    krb5_context        context;
    krb5_error_code     kret;
    struct iterate_args *iargs;
    krb5_keyblock       key;
    krb5_int32          ind;
    krb5_data   pwd;
    krb5_db_entry       *entry;

    iargs = (struct iterate_args *) ptr;
    kret = 0;

    context = iargs->ctx;
    entry = iargs->dbentp;

    /*
     * Convert the master key password into a key for this particular
     * encryption system.
     */
    pwd.data = mkey_password;
    pwd.length = strlen(mkey_password);
    kret = krb5_c_random_seed(context, &pwd);
    if (kret)
        return kret;

    /*if (!(kret = krb5_dbe_create_key_data(iargs->ctx, iargs->dbentp))) {*/
    if ((entry->key_data =
         (krb5_key_data *) realloc(entry->key_data,
                                   (sizeof(krb5_key_data) *
                                    (entry->n_key_data + 1)))) == NULL)
        return (ENOMEM);

    memset(entry->key_data + entry->n_key_data, 0, sizeof(krb5_key_data));
    ind = entry->n_key_data++;

    if (!(kret = krb5_c_make_random_key(context, ksent->ks_enctype,
                                        &key))) {
        kret = krb5_dbe_encrypt_key_data(context, iargs->rblock->key, &key,
                                         NULL, 1, &entry->key_data[ind]);
        krb5_free_keyblock_contents(context, &key);
    }
    /*}*/

    return(kret);
}
/* End duplicate code */

/*
 * This function creates service principals when
 * creating the realm object.
 */
static int
kdb_ldap_create_principal(krb5_context context, krb5_principal princ,
                          enum ap_op op, struct realm_info *pblock,
                          const krb5_keyblock *master_keyblock)
{
    int              retval=0, currlen=0, princtype = 2 /* Service Principal */;
    unsigned char    *curr=NULL;
    krb5_tl_data     *tl_data=NULL;
    krb5_db_entry    entry;
    long             mask = 0;
    krb5_keyblock    key;
    int              kvno = 0;
    kdb5_dal_handle *dal_handle = NULL;
    krb5_ldap_context *ldap_context=NULL;
    struct iterate_args   iargs;
    krb5_data       *pdata;
    krb5_timestamp now;
    krb5_actkvno_node     actkvno;

    memset(&entry, 0, sizeof(entry));

    if ((pblock == NULL) || (context == NULL)) {
        retval = EINVAL;
        goto cleanup;
    }
    dal_handle = context->dal_handle;
    ldap_context = (krb5_ldap_context *) dal_handle->db_context;
    if (!(ldap_context)) {
        retval = EINVAL;
        goto cleanup;
    }

    tl_data = malloc(sizeof(*tl_data));
    if (tl_data == NULL) {
        retval = ENOMEM;
        goto cleanup;
    }
    memset(tl_data, 0, sizeof(*tl_data));
    tl_data->tl_data_length = 1 + 2 + 2 + 1 + 2 + 4;
    tl_data->tl_data_type = 7; /* KDB_TL_USER_INFO */
    curr = tl_data->tl_data_contents = malloc(tl_data->tl_data_length);
    if (tl_data->tl_data_contents == NULL) {
        retval = ENOMEM;
        goto cleanup;
    }

    memset(curr, 1, 1); /* Passing the mask as principal type */
    curr += 1;
    currlen = 2;
    STORE16_INT(curr, currlen);
    curr += currlen;
    STORE16_INT(curr, princtype);
    curr += currlen;

    mask |= KADM5_PRINCIPAL;
    mask |= KADM5_ATTRIBUTES ;
    mask |= KADM5_MAX_LIFE ;
    mask |= KADM5_MAX_RLIFE ;
    mask |= KADM5_PRINC_EXPIRE_TIME ;
    mask |= KADM5_KEY_DATA;

    entry.tl_data = tl_data;
    entry.n_tl_data += 1;
    /* Set the creator's name */
    if ((retval = krb5_timeofday(context, &now)))
        goto cleanup;
    if ((retval = krb5_dbe_update_mod_princ_data_new(context, &entry,
                                                     now, &db_create_princ)))
        goto cleanup;

    entry.attributes = pblock->flags | KRB5_KDB_LOCKDOWN_KEYS;
    entry.max_life = pblock->max_life;
    entry.max_renewable_life = pblock->max_rlife;
    entry.expiration = pblock->expiration;
    entry.mask = mask;
    if ((retval = krb5_copy_principal(context, princ, &entry.princ)))
        goto cleanup;


    switch (op) {
    case TGT_KEY:
        if ((pdata = krb5_princ_component(context, princ, 1)) &&
            pdata->length == strlen("history") &&
            !memcmp(pdata->data, "history", strlen("history"))) {

            /* Allocate memory for storing the key */
            if ((entry.key_data = (krb5_key_data *) malloc(
                     sizeof(krb5_key_data))) == NULL) {
                retval = ENOMEM;
                goto cleanup;
            }

            memset(entry.key_data, 0, sizeof(krb5_key_data));
            entry.n_key_data++;

            retval = krb5_c_make_random_key(context, global_params.enctype, &key);
            if (retval) {
                goto cleanup;
            }
            kvno = 1; /* New key is getting set */
            retval = krb5_dbe_encrypt_key_data(context, master_keyblock,
                                               &key, NULL, kvno,
                                               &entry.key_data[entry.n_key_data - 1]);
            krb5_free_keyblock_contents(context, &key);
            if (retval) {
                goto cleanup;
            }
        } else {
            /*retval = krb5_c_make_random_key(context, 16, &key) ;*/
            iargs.ctx = context;
            iargs.rblock = pblock;
            iargs.dbentp = &entry;

            /*
             * Iterate through the key/salt list, ignoring salt types.
             */
            if ((retval = krb5_keysalt_iterate(pblock->kslist,
                                               pblock->nkslist,
                                               1,
                                               kdb_ldap_tgt_keysalt_iterate,
                                               (krb5_pointer) &iargs)))
                return retval;
        }
        break;

    case MASTER_KEY:
        /* Allocate memory for storing the key */
        if ((entry.key_data = (krb5_key_data *) malloc(
                 sizeof(krb5_key_data))) == NULL) {
            retval = ENOMEM;
            goto cleanup;
        }

        memset(entry.key_data, 0, sizeof(krb5_key_data));
        entry.n_key_data++;
        kvno = 1; /* New key is getting set */
        retval = krb5_dbe_encrypt_key_data(context, pblock->key,
                                           master_keyblock, NULL, kvno,
                                           &entry.key_data[entry.n_key_data - 1]);
        if (retval) {
            goto cleanup;
        }
        /*
         * There should always be at least one "active" mkey so creating the
         * KRB5_TL_ACTKVNO entry now so the initial mkey is active.
         */
        actkvno.next = NULL;
        actkvno.act_kvno = kvno;
        actkvno.act_time = now;
        retval = krb5_dbe_update_actkvno(context, &entry, &actkvno);
        if (retval)
            goto cleanup;

        break;

    case NULL_KEY:
    default:
        break;
    } /* end of switch */

    retval = krb5_ldap_put_principal(context, &entry, NULL);
    if (retval) {
        com_err(NULL, retval, _("while adding entries to database"));
        goto cleanup;
    }

cleanup:
    krb5_dbe_free_contents(context, &entry);
    return retval;
}


/*
 * This function destroys the realm object and the associated principals
 */
void
kdb5_ldap_destroy(int argc, char *argv[])
{
    extern char *optarg;
    extern int optind;
    int optchar = 0;
    char buf[5] = {0};
    krb5_error_code retval = 0;
    int force = 0;
    int mask = 0;
    kdb5_dal_handle *dal_handle = NULL;
    krb5_ldap_context *ldap_context = NULL;

    optind = 1;
    while ((optchar = getopt(argc, argv, "f")) != -1) {
        switch (optchar) {
        case 'f':
            force++;
            break;
        case '?':
        default:
            db_usage(DESTROY_REALM);
            return;
            /*NOTREACHED*/
        }
    }

    if (!force) {
        printf(_("Deleting KDC database of '%s', are you sure?\n"),
               global_params.realm);
        printf(_("(type 'yes' to confirm)? "));
        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            exit_status++;
            return;
        }
        if (strcmp(buf, yes)) {
            exit_status++;
            return;
        }
        printf(_("OK, deleting database of '%s'...\n"), global_params.realm);
    }

    dal_handle = util_context->dal_handle;
    ldap_context = (krb5_ldap_context *) dal_handle->db_context;
    if (!(ldap_context)) {
        com_err(progname, EINVAL, _("while initializing database"));
        exit_status++;
        return;
    }

    /* Read the kerberos container DN */
    retval = krb5_ldap_read_krbcontainer_dn(util_context,
                                            &ldap_context->container_dn);
    if (retval) {
        com_err(progname, retval,
                _("while reading kerberos container information"));
        exit_status++;
        return;
    }

    /* Read the Realm information from the LDAP Server */
    if ((retval = krb5_ldap_read_realm_params(util_context, global_params.realm,
                                              &(ldap_context->lrparams), &mask)) != 0) {
        com_err(progname, retval, _("while reading realm information"));
        exit_status++;
        return;
    }

    /* Delete the realm container and all the associated principals */
    retval = krb5_ldap_delete_realm(util_context, global_params.realm);
    if (retval) {
        com_err(progname, retval,
                _("deleting database of '%s'"), global_params.realm);
        exit_status++;
        return;
    }

    printf(_("** Database of '%s' destroyed.\n"), global_params.realm);

    return;
}
