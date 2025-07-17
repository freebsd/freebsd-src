/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kadmin/dbutil/kdb5_util.c - Administer a KDC database */
/*
 * (C) Copyright 1990,1991, 1996, 2008, 2009 by the Massachusetts Institute of Technology.
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
/*
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <k5-int.h>
#include <kadm5/admin.h>
#include <locale.h>
#include <adm_proto.h>
#include <time.h>
#include "kdb5_util.h"

/*
 * XXX Ick, ick, ick.  These global variables shouldn't be global....
 */
char *mkey_password = 0;

/*
 * I can't figure out any way for this not to be global, given how ss
 * works.
 */

int exit_status = 0;
krb5_context util_context;
kadm5_config_params global_params;

void usage()
{
    fprintf(stderr,
            _("Usage: kdb5_util [-r realm] [-d dbname] "
              "[-k mkeytype] [-kv mkeyVNO]\n"
              "\t        [-M mkeyname] [-m] [-sf stashfilename] "
              "[-P password]\n"
              "\t        [-x db_args]* cmd [cmd_options]\n"
              "\tcreate  [-s]\n"
              "\tdestroy [-f]\n"
              "\tstash   [-f keyfile]\n"
              "\tdump    [-b7|-r13|-r18] [-verbose]\n"
              "\t        [-mkey_convert] [-new_mkey_file mkey_file]\n"
              "\t        [-rev] [-recurse] [filename [princs...]]\n"
              "\tload    [-b7|-r13|-r18] [-hash] [-verbose] [-update] "
              "filename\n"
              "\tark     [-e etype_list] principal\n"
              "\tadd_mkey [-e etype] [-s]\n"
              "\tuse_mkey kvno [time]\n"
              "\tlist_mkeys\n"));
    /* avoid a string length compiler warning */
    fprintf(stderr,
            _("\tupdate_princ_encryption [-f] [-n] [-v] [princ-pattern]\n"
              "\tpurge_mkeys [-f] [-n] [-v]\n"
              "\ttabdump [-H] [-c] [-e] [-n] [-o outfile] dumptype\n"
              "\nwhere,\n\t[-x db_args]* - any number of database specific "
              "arguments.\n"
              "\t\t\tLook at each database documentation for supported "
              "arguments\n"));
    exit(1);
}

krb5_keyblock master_keyblock;
krb5_kvno   master_kvno; /* fetched */
extern krb5_principal master_princ;
char *mkey_fullname;
krb5_db_entry *master_entry = NULL;
int     valid_master_key = 0;

char *progname;
krb5_boolean manual_mkey = FALSE;
krb5_boolean dbactive = FALSE;

static int open_db_and_mkey(void);

static void add_random_key(int, char **);

typedef void (*cmd_func)(int, char **);

struct _cmd_table {
    char *name;
    cmd_func func;
    int opendb;
} cmd_table[] = {
    {"create", kdb5_create, 0},
    {"destroy", kdb5_destroy, 1}, /* 1 opens the kdb */
    {"stash", kdb5_stash, 1},
    {"dump", dump_db, 1},
    {"load", load_db, 0},
    {"ark", add_random_key, 1},
    {"add_mkey", kdb5_add_mkey, 1},
    {"use_mkey", kdb5_use_mkey, 1},
    {"list_mkeys", kdb5_list_mkeys, 1},
    {"update_princ_encryption", kdb5_update_princ_encryption, 1},
    {"purge_mkeys", kdb5_purge_mkeys, 1},
    {"tabdump", tabdump, 1},
    {NULL, NULL, 0},
};

static struct _cmd_table *cmd_lookup(name)
    char *name;
{
    struct _cmd_table *cmd = cmd_table;
    while (cmd->name) {
        if (strcmp(cmd->name, name) == 0)
            return cmd;
        else
            cmd++;
    }

    return NULL;
}

#define ARG_VAL (--argc > 0 ? (koptarg = *(++argv)) : (char *)(usage(), NULL))

char **db5util_db_args = NULL;
int    db5util_db_args_size = 0;

static void extended_com_err_fn (const char *myprog, errcode_t code,
                                 const char *fmt, va_list args)
{
    const char *emsg;
    if (code) {
        emsg = krb5_get_error_message (util_context, code);
        fprintf (stderr, "%s: %s ", myprog, emsg);
        krb5_free_error_message (util_context, emsg);
    } else {
        fprintf (stderr, "%s: ", myprog);
    }
    vfprintf (stderr, fmt, args);
    fprintf (stderr, "\n");
}

int add_db_arg(char *arg)
{
    char **temp;
    db5util_db_args_size++;
    temp = realloc(db5util_db_args,
                   sizeof(char *) * (db5util_db_args_size + 1));
    if (temp == NULL)
        return 0;
    db5util_db_args = temp;
    db5util_db_args[db5util_db_args_size-1] = arg;
    db5util_db_args[db5util_db_args_size]   = NULL;
    return 1;
}

int main(argc, argv)
    int argc;
    char *argv[];
{
    struct _cmd_table *cmd = NULL;
    char *koptarg, **cmd_argv;
    char *db_name_tmp = NULL;
    int cmd_argc;
    krb5_error_code retval;

    setlocale(LC_ALL, "");
    set_com_err_hook(extended_com_err_fn);

    /*
     * Ensure that "progname" is set before calling com_err.
     */
    progname = (strrchr(argv[0], '/') ?
                strrchr(argv[0], '/') + 1 : argv[0]);

    retval = kadm5_init_krb5_context(&util_context);
    if (retval) {
        com_err (progname, retval, _("while initializing Kerberos code"));
        exit(1);
    }

    cmd_argv = (char **) malloc(sizeof(char *)*argc);
    if (cmd_argv == NULL) {
        com_err(progname, ENOMEM, _("while creating sub-command arguments"));
        exit(1);
    }
    memset(cmd_argv, 0, sizeof(char *)*argc);
    cmd_argc = 0;

    argv++; argc--;
    while (*argv) {
        if (strcmp(*argv, "-P") == 0 && ARG_VAL) {
            mkey_password = koptarg;
            manual_mkey = TRUE;
        } else if (strcmp(*argv, "-d") == 0 && ARG_VAL) {
            global_params.dbname = koptarg;
            global_params.mask |= KADM5_CONFIG_DBNAME;

            if (asprintf(&db_name_tmp, "dbname=%s", global_params.dbname) < 0)
            {
                com_err(progname, ENOMEM,
                        _("while parsing command arguments"));
                exit(1);
            }

            if (!add_db_arg(db_name_tmp)) {
                com_err(progname, ENOMEM,
                        _("while parsing command arguments\n"));
                exit(1);
            }

        } else if (strcmp(*argv, "-x") == 0 && ARG_VAL) {
            if (!add_db_arg(koptarg)) {
                com_err(progname, ENOMEM,
                        _("while parsing command arguments\n"));
                exit(1);
            }

        } else if (strcmp(*argv, "-r") == 0 && ARG_VAL) {
            global_params.realm = koptarg;
            global_params.mask |= KADM5_CONFIG_REALM;
            /* not sure this is really necessary */
            if ((retval = krb5_set_default_realm(util_context,
                                                 global_params.realm))) {
                com_err(progname, retval,
                        _("while setting default realm name"));
                exit(1);
            }
        } else if (strcmp(*argv, "-k") == 0 && ARG_VAL) {
            if (krb5_string_to_enctype(koptarg, &global_params.enctype)) {
                com_err(progname, EINVAL, _(": %s is an invalid enctype"),
                        koptarg);
                exit(1);
            } else
                global_params.mask |= KADM5_CONFIG_ENCTYPE;
        } else if (strcmp(*argv, "-kv") == 0 && ARG_VAL) {
            global_params.kvno = (krb5_kvno) atoi(koptarg);
            if (global_params.kvno == IGNORE_VNO) {
                com_err(progname, EINVAL, _(": %s is an invalid mkeyVNO"),
                        koptarg);
                exit(1);
            } else
                global_params.mask |= KADM5_CONFIG_KVNO;
        } else if (strcmp(*argv, "-M") == 0 && ARG_VAL) {
            global_params.mkey_name = koptarg;
            global_params.mask |= KADM5_CONFIG_MKEY_NAME;
        } else if (strcmp(*argv, "-sf") == 0 && ARG_VAL) {
            global_params.stash_file = koptarg;
            global_params.mask |= KADM5_CONFIG_STASH_FILE;
        } else if (strcmp(*argv, "-m") == 0) {
            manual_mkey = TRUE;
            global_params.mkey_from_kbd = 1;
            global_params.mask |= KADM5_CONFIG_MKEY_FROM_KBD;
        } else {
            cmd_argv[cmd_argc++] = *argv;
        }
        argv++; argc--;
    }

    if (cmd_argv[0] == NULL)
        usage();
    cmd = cmd_lookup(cmd_argv[0]);
    if (cmd == NULL)
        usage();

    if( !util_context->default_realm )
    {
        char *temp = NULL;
        retval = krb5_get_default_realm(util_context, &temp);
        if( retval )
        {
            com_err(progname, retval, _("while getting default realm"));
            exit(1);
        }
        krb5_free_default_realm(util_context, temp);
    }

    retval = kadm5_get_config_params(util_context, 1,
                                     &global_params, &global_params);
    if (retval) {
        com_err(progname, retval,
                _("while retrieving configuration parameters"));
        exit(1);
    }

    /*
     * Dump creates files which should not be world-readable.  It is
     * easiest to do a single umask call here.
     */
    (void) umask(077);

    master_keyblock.enctype = global_params.enctype;
    if ((master_keyblock.enctype != ENCTYPE_UNKNOWN) &&
        (!krb5_c_valid_enctype(master_keyblock.enctype))) {
        com_err(progname, KRB5_PROG_KEYTYPE_NOSUPP,
                "while setting up enctype %d", master_keyblock.enctype);
    }

    if (cmd->opendb && open_db_and_mkey())
        return exit_status;

    if (global_params.iprop_enabled == TRUE)
        ulog_set_role(util_context, IPROP_PRIMARY);
    else
        ulog_set_role(util_context, IPROP_NULL);

    (*cmd->func)(cmd_argc, cmd_argv);

    if( db_name_tmp )
        free( db_name_tmp );

    if( db5util_db_args )
        free(db5util_db_args);

    quit();
    kadm5_free_config_params(util_context, &global_params);
    krb5_free_context(util_context);
    free(cmd_argv);
    return exit_status;
}

/*
 * open_db_and_mkey: Opens the KDC and policy database, and sets the
 * global master_* variables.  Sets dbactive to TRUE if the databases
 * are opened, and valid_master_key to 1 if the global master
 * variables are set properly.  Returns 0 on success, and 1 on
 * failure, but it is not considered a failure if the master key
 * cannot be fetched (the master key stash file may not exist when the
 * program is run).
 */
static int open_db_and_mkey()
{
    krb5_error_code retval;
    krb5_data scratch, pwd, seed;

    dbactive = FALSE;
    valid_master_key = 0;

    if ((retval = krb5_db_open(util_context, db5util_db_args,
                               KRB5_KDB_OPEN_RW | KRB5_KDB_SRV_TYPE_ADMIN))) {
        com_err(progname, retval, _("while initializing database"));
        exit_status++;
        return(1);
    }

    /* assemble & parse the master key name */

    if ((retval = krb5_db_setup_mkey_name(util_context,
                                          global_params.mkey_name,
                                          global_params.realm,
                                          &mkey_fullname, &master_princ))) {
        com_err(progname, retval, _("while setting up master key name"));
        exit_status++;
        return(1);
    }
    if ((retval = krb5_db_get_principal(util_context, master_princ, 0,
                                        &master_entry))) {
        com_err(progname, retval, _("while retrieving master entry"));
        exit_status++;
        (void) krb5_db_fini(util_context);
        return(1);
    }

    if (global_params.mask & KADM5_CONFIG_KVNO)
        master_kvno = global_params.kvno; /* user specified */
    else
        master_kvno = IGNORE_VNO;

    /* the databases are now open, and the master principal exists */
    dbactive = TRUE;

    if (mkey_password) {
        pwd.data = mkey_password;
        pwd.length = strlen(mkey_password);
        retval = krb5_principal2salt(util_context, master_princ, &scratch);
        if (retval) {
            com_err(progname, retval, _("while calculated master key salt"));
            exit_status++;
            return(1);
        }

        /* If no encryption type is set, use the default */
        if (master_keyblock.enctype == ENCTYPE_UNKNOWN)
            master_keyblock.enctype = DEFAULT_KDC_ENCTYPE;
        if (!krb5_c_valid_enctype(master_keyblock.enctype))
            com_err(progname, KRB5_PROG_KEYTYPE_NOSUPP,
                    "while setting up enctype %d",
                    master_keyblock.enctype);

        retval = krb5_c_string_to_key(util_context, master_keyblock.enctype,
                                      &pwd, &scratch, &master_keyblock);
        if (retval) {
            com_err(progname, retval,
                    _("while transforming master key from password"));
            exit_status++;
            return(1);
        }
        free(scratch.data);
        mkey_password = 0;

    } else {
        if ((retval = krb5_db_fetch_mkey(util_context, master_princ,
                                         master_keyblock.enctype,
                                         manual_mkey, FALSE,
                                         global_params.stash_file,
                                         &master_kvno,
                                         0, &master_keyblock))) {
            com_err(progname, retval, _("while reading master key"));
            com_err(progname, 0, _("Warning: proceeding without master key"));
            exit_status++;
            return(0);
        }
    }

    if ((retval = krb5_db_fetch_mkey_list(util_context, master_princ,
                                          &master_keyblock))) {
        com_err(progname, retval, "while getting master key list");
        com_err(progname, 0, "Warning: proceeding without master key list");
        exit_status++;
        return(0);
    }

    seed.length = master_keyblock.length;
    seed.data = (char *) master_keyblock.contents;

    if ((retval = krb5_c_random_seed(util_context, &seed))) {
        com_err(progname, retval, _("while seeding random number generator"));
        exit_status++;
        memset(master_keyblock.contents, 0, master_keyblock.length);
        krb5_free_keyblock_contents(util_context, &master_keyblock);
        return(1);
    }

    if (global_params.iprop_enabled) {
        if (ulog_map(util_context, global_params.iprop_logfile,
                     global_params.iprop_ulogsize)) {
            fprintf(stderr, _("%s: Could not map log\n"), progname);
            exit_status++;
            return(1);
        }
    }

    valid_master_key = 1;
    dbactive = TRUE;
    return 0;
}

#ifdef HAVE_GETCWD
#undef getwd
#endif

int
quit()
{
    krb5_error_code retval;
    static krb5_boolean finished = 0;

    if (finished)
        return 0;
    ulog_fini(util_context);
    retval = krb5_db_fini(util_context);
    zapfree(master_keyblock.contents, master_keyblock.length);
    krb5_free_principal(util_context, master_princ);
    finished = TRUE;
    if (retval && retval != KRB5_KDB_DBNOTINITED) {
        com_err(progname, retval, _("while closing database"));
        exit_status++;
        return 1;
    }
    return 0;
}

static void
add_random_key(argc, argv)
    int argc;
    char **argv;
{
    krb5_error_code ret;
    krb5_principal princ;
    krb5_db_entry *dbent;
    krb5_timestamp now;

    krb5_key_salt_tuple *keysalts = NULL;
    krb5_int32 num_keysalts = 0;

    int free_keysalts;
    char *me = progname;
    char *ks_str = NULL;
    char *pr_str;
    krb5_keyblock *tmp_mkey;

    if (argc < 2)
        usage();
    for (argv++, argc--; *argv; argv++, argc--) {
        if (!strcmp(*argv, "-e")) {
            argv++; argc--;
            ks_str = *argv;
            continue;
        } else
            break;
    }
    if (argc < 1)
        usage();
    pr_str = *argv;
    ret = krb5_parse_name(util_context, pr_str, &princ);
    if (ret) {
        com_err(me, ret, _("while parsing principal name %s"), pr_str);
        exit_status++;
        return;
    }
    ret = krb5_db_get_principal(util_context, princ, 0, &dbent);
    if (ret) {
        com_err(me, ret, _("while fetching principal %s"), pr_str);
        exit_status++;
        return;
    }
    ret = krb5_string_to_keysalts(ks_str,
                                  NULL, NULL, 0,
                                  &keysalts,
                                  &num_keysalts);
    if (ret) {
        com_err(me, ret, _("while parsing keysalts %s"), ks_str);
        exit_status++;
        return;
    }
    if (!num_keysalts || keysalts == NULL) {
        num_keysalts = global_params.num_keysalts;
        keysalts = global_params.keysalts;
        free_keysalts = 0;
    } else
        free_keysalts = 1;

    /* Find the mkey used to protect the existing keys */
    ret = krb5_dbe_find_mkey(util_context, dbent, &tmp_mkey);
    if (ret) {
        com_err(me, ret, _("while finding mkey"));
        krb5_db_free_principal(util_context, dbent);
        exit_status++;
        return;
    }

    ret = krb5_dbe_ark(util_context, tmp_mkey, keysalts, num_keysalts, dbent);
    if (free_keysalts)
        free(keysalts);
    if (ret) {
        com_err(me, ret, "while randomizing principal %s", pr_str);
        krb5_db_free_principal(util_context, dbent);
        exit_status++;
        return;
    }
    dbent->attributes &= ~KRB5_KDB_REQUIRES_PWCHANGE;
    ret = krb5_timeofday(util_context, &now);
    if (ret) {
        com_err(me, ret, _("while getting time"));
        krb5_db_free_principal(util_context, dbent);
        exit_status++;
        return;
    }
    ret = krb5_dbe_update_last_pwd_change(util_context, dbent, now);
    if (ret) {
        com_err(me, ret, _("while setting changetime"));
        krb5_db_free_principal(util_context, dbent);
        exit_status++;
        return;
    }
    ret = krb5_db_put_principal(util_context, dbent);
    krb5_db_free_principal(util_context, dbent);
    if (ret) {
        com_err(me, ret, _("while saving principal %s"), pr_str);
        exit_status++;
        return;
    }
    printf(_("%s changed\n"), pr_str);
}
