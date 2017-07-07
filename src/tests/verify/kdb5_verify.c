/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/verify/kdb5_verify.c */
/*
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "kdb.h"
#include "com_err.h"
#include <ss/ss.h>
#include <stdio.h>

#define REALM_SEP       '@'
#define REALM_SEP_STR   "@"

struct mblock {
    krb5_deltat max_life;
    krb5_deltat max_rlife;
    krb5_timestamp expiration;
    krb5_flags flags;
    krb5_kvno mkvno;
} mblock = {                            /* XXX */
    KRB5_KDB_MAX_LIFE,
    KRB5_KDB_MAX_RLIFE,
    KRB5_KDB_EXPIRATION,
    KRB5_KDB_DEF_FLAGS,
    0
};

int set_dbname_help (krb5_context, char *, char *);

static void
usage(who, status)
    char *who;
    int status;
{
    fprintf(stderr,
            "usage: %s -p prefix -n num_to_check [-d dbpathname] [-r realmname]\n",
            who);
    fprintf(stderr, "\t [-D depth] [-k enctype] [-M mkeyname]\n");

    exit(status);
}

krb5_keyblock master_keyblock;
krb5_principal master_princ;
krb5_encrypt_block master_encblock;
krb5_pointer master_random;
char *str_master_princ;

static char *progname;
static char *cur_realm = 0;
static char *mkey_name = 0;
static char *mkey_password = 0;
static krb5_boolean manual_mkey = FALSE;


int check_princ (krb5_context, char *);

int
main(argc, argv)
    int argc;
    char *argv[];
{
    extern char *optarg;
    int optchar, i, n;
    char tmp[4096], tmp2[BUFSIZ], *str_princ;

    krb5_context context;
    krb5_error_code retval;
    char *dbname = 0;
    int enctypedone = 0;
    int num_to_check;
    char principal_string[BUFSIZ];
    char *suffix = 0;
    size_t suffix_size = 0;
    int depth, errors;

    krb5_init_context(&context);

    if (strrchr(argv[0], '/'))
        argv[0] = strrchr(argv[0], '/')+1;

    progname = argv[0];

    memset(principal_string, 0, sizeof(principal_string));
    num_to_check = 0;
    depth = 1;

    while ((optchar = getopt(argc, argv, "D:P:p:n:d:r:R:k:M:e:m")) != -1) {
        switch(optchar) {
        case 'D':
            depth = atoi(optarg);       /* how deep to go */
            break;
        case 'P':               /* Only used for testing!!! */
            mkey_password = optarg;
            break;
        case 'p':                       /* prefix name to check */
            strncpy(principal_string, optarg, sizeof(principal_string) - 1);
            principal_string[sizeof(principal_string) - 1] = '\0';
            suffix = principal_string + strlen(principal_string);
            suffix_size = sizeof(principal_string) -
                (suffix - principal_string);
            break;
        case 'n':                        /* how many to check */
            num_to_check = atoi(optarg);
            break;
        case 'd':                       /* set db name */
            dbname = optarg;
            break;
        case 'r':
            cur_realm = optarg;
            break;
        case 'k':
            master_keyblock.enctype = atoi(optarg);
            enctypedone++;
            break;
        case 'M':                       /* master key name in DB */
            mkey_name = optarg;
            break;
        case 'm':
            manual_mkey = TRUE;
            break;
        case '?':
        default:
            usage(progname, 1);
            /*NOTREACHED*/
        }
    }

    if (!(num_to_check && suffix)) usage(progname, 1);

    if (!enctypedone)
        master_keyblock.enctype = DEFAULT_KDC_ENCTYPE;

    if (!krb5_c_valid_enctype(master_keyblock.enctype)) {
        com_err(progname, KRB5_PROG_ETYPE_NOSUPP,
                "while setting up enctype %d", master_keyblock.enctype);
        exit(1);
    }

    krb5_use_enctype(context, &master_encblock, master_keyblock.enctype);

    if (!dbname)
        dbname = DEFAULT_KDB_FILE;      /* XXX? */

    if (!cur_realm) {
        if ((retval = krb5_get_default_realm(context, &cur_realm))) {
            com_err(progname, retval, "while retrieving default realm name");
            exit(1);
        }
    }
    if ((retval = set_dbname_help(context, progname, dbname)))
        exit(retval);

    errors = 0;

    fprintf(stdout, "\nChecking ");

    for (n = 1; n <= num_to_check; n++) {
        /* build the new principal name */
        /* we can't pick random names because we need to generate all the names
           again given a prefix and count to test the db lib and kdb */
        (void) snprintf(suffix, suffix_size, "%d", n);
        (void) snprintf(tmp, sizeof(tmp), "%s-DEPTH-1", principal_string);
        str_princ = tmp;
        if (check_princ(context, str_princ)) errors++;

        for (i = 2; i <= depth; i++) {
            (void) snprintf(tmp2, sizeof(tmp2), "/%s-DEPTH-%d",
                            principal_string, i);
            tmp2[sizeof(tmp2) - 1] = '\0';
            strncat(tmp, tmp2, sizeof(tmp) - 1 - strlen(tmp));
            str_princ = tmp;
            if (check_princ(context, str_princ)) errors++;
        }
    }

    if (errors)
        fprintf(stdout, "\n%d errors/principals failed.\n", errors);
    else
        fprintf(stdout, "\nNo errors.\n");

    krb5_finish_random_key(context, &master_encblock, &master_random);
    krb5_finish_key(context, &master_encblock);

    retval = krb5_db_fini(context);
    memset(master_keyblock.contents, 0, (size_t) master_keyblock.length);
    if (retval && retval != KRB5_KDB_DBNOTINITED) {
        com_err(progname, retval, "while closing database");
        exit(1);
    }
    krb5_free_keyblock_contents(context, &master_keyblock);

    if (str_master_princ) {
        krb5_free_unparsed_name(context, str_master_princ);
    }
    krb5_free_principal(context, master_princ);
    krb5_free_context(context);
    exit(0);
}

int
check_princ(context, str_princ)
    krb5_context context;
    char * str_princ;
{
    krb5_error_code retval;
    krb5_db_entry *kdbe = NULL;
    krb5_keyblock pwd_key, db_key;
    krb5_data pwd, salt;
    krb5_principal princ;
    /* char *str_mod_name; */
    char princ_name[4096];

    snprintf(princ_name, sizeof(princ_name), "%s@%s", str_princ, cur_realm);

    if ((retval = krb5_parse_name(context, princ_name, &princ))) {
        com_err(progname, retval, "while parsing '%s'", princ_name);
        goto out;
    }

    pwd.data = princ_name;  /* must be able to regenerate */
    pwd.length = strlen(princ_name);

    if ((retval = krb5_principal2salt(context, princ, &salt))) {
        com_err(progname, retval, "while converting principal to salt for '%s'", princ_name);
        krb5_free_principal(context, princ);
        goto out;
    }

    if ((retval = krb5_string_to_key(context, &master_encblock,
                                     &pwd_key, &pwd, &salt))) {
        com_err(progname, retval, "while converting password to key for '%s'",
                princ_name);
        krb5_free_data_contents(context, &salt);
        krb5_free_principal(context, princ);
        goto out;
    }
    krb5_free_data_contents(context, &salt);

    if ((retval = krb5_db_get_principal(context, princ, 0, &kdbe))) {
        com_err(progname, retval, "while attempting to verify principal's existence");
        krb5_free_principal(context, princ);
        goto out;
    }
    krb5_free_principal(context, princ);

    if ((retval = krb5_dbe_decrypt_key_data(context, NULL,
                                            kdbe->key_data, &db_key, NULL))) {
        com_err(progname, retval, "while decrypting key for '%s'", princ_name);
        goto errout;
    }

    if ((pwd_key.enctype != db_key.enctype) ||
        (pwd_key.length != db_key.length)) {
        fprintf (stderr, "\tKey types do not agree (%d expected, %d from db)\n",
                 pwd_key.enctype, db_key.enctype);
    errout:
        krb5_db_free_principal(context, kdbe);
        return(-1);
    }
    else {
        if (memcmp((char *)pwd_key.contents, (char *) db_key.contents,
                   (size_t) pwd_key.length)) {
            fprintf(stderr, "\t key did not match stored value for %s\n",
                    princ_name);
            goto errout;
        }
    }

    free(pwd_key.contents);
    free(db_key.contents);

    if (kdbe->key_data[0].key_data_kvno != 1) {
        fprintf(stderr,"\tkvno did not match stored value for %s.\n", princ_name);
        goto errout;
    }

    if (kdbe->max_life != mblock.max_life) {
        fprintf(stderr, "\tmax life did not match stored value for %s.\n",
                princ_name);
        goto errout;
    }

    if (kdbe->max_renewable_life != mblock.max_rlife) {
        fprintf(stderr,
                "\tmax renewable life did not match stored value for %s.\n",
                princ_name);
        goto errout;
    }

    if (kdbe->expiration != mblock.expiration) {
        fprintf(stderr, "\texpiration time did not match stored value for %s.\n",
                princ_name);
        goto errout;
    }

/*
  if ((retval = krb5_unparse_name(context, kdbe.mod_name, &str_mod_name)))
  com_err(progname, retval, "while unparsing mode name");
  else {
  if (strcmp(str_mod_name, str_master_princ) != 0) {
  fprintf(stderr, "\tmod name isn't the master princ (%s not %s).\n",
  str_mod_name, str_master_princ);
  free(str_mod_name);
  goto errout;
  }
  else free(str_mod_name);
  }
*/

    if (kdbe->attributes != mblock.flags) {
        fprintf(stderr, "\tAttributes did not match stored value for %s.\n",
                princ_name);
        goto errout;
    }

out:
    krb5_db_free_principal(context, kdbe);

    return(0);
}

int
set_dbname_help(context, pname, dbname)
    krb5_context context;
    char *pname;
    char *dbname;
{
    krb5_error_code retval;
    krb5_data pwd, scratch;
    char *args[2];
    krb5_db_entry *master_entry;

    /* assemble & parse the master key name */

    if ((retval = krb5_db_setup_mkey_name(context, mkey_name, cur_realm, 0,
                                          &master_princ))) {
        com_err(pname, retval, "while setting up master key name");
        return(1);
    }
    if (mkey_password) {
        pwd.data = mkey_password;
        pwd.length = strlen(mkey_password);
        retval = krb5_principal2salt(context, master_princ, &scratch);
        if (retval) {
            com_err(pname, retval, "while calculated master key salt");
            return(1);
        }
        if ((retval = krb5_string_to_key(context, &master_encblock,
                                         &master_keyblock, &pwd, &scratch))) {
            com_err(pname, retval,
                    "while transforming master key from password");
            return(1);
        }
        free(scratch.data);
    } else {
        if ((retval = krb5_db_fetch_mkey(context, master_princ,
                                         master_keyblock.enctype,
                                         manual_mkey, FALSE, (char *) NULL,
                                         NULL, NULL,
                                         &master_keyblock))) {
            com_err(pname, retval, "while reading master key");
            return(1);
        }
    }

    /* Ick!  Current DAL interface requires that the default_realm
       field be set in the krb5_context.  */
    if ((retval = krb5_set_default_realm(context, cur_realm))) {
        com_err(pname, retval, "setting default realm");
        return 1;
    }
    /* Pathname is passed to db2 via 'args' parameter.  */
    args[1] = NULL;
    if (asprintf(&args[0], "dbname=%s", dbname) < 0) {
        com_err(pname, errno, "while setting up db parameters");
        return 1;
    }

    if ((retval = krb5_db_open(context, args, KRB5_KDB_OPEN_RO))) {
        com_err(pname, retval, "while initializing database");
        return(1);
    }
    if ((retval = krb5_db_fetch_mkey_list(context, master_princ,
                                          &master_keyblock))) {
        com_err(pname, retval, "while verifying master key");
        (void) krb5_db_fini(context);
        return(1);
    }
    if ((retval = krb5_db_get_principal(context, master_princ, 0,
                                        &master_entry))) {
        com_err(pname, retval, "while retrieving master entry");
        (void) krb5_db_fini(context);
        return(1);
    }

    if ((retval = krb5_unparse_name(context, master_princ,
                                    &str_master_princ))) {
        com_err(pname, retval, "while unparsing master principal");
        krb5_db_fini(context);
        return(1);
    }

    if ((retval = krb5_process_key(context,
                                   &master_encblock, &master_keyblock))) {
        com_err(pname, retval, "while processing master key");
        (void) krb5_db_fini(context);
        return(1);
    }
    if ((retval = krb5_init_random_key(context,
                                       &master_encblock, &master_keyblock,
                                       &master_random))) {
        com_err(pname, retval, "while initializing random key generator");
        krb5_finish_key(context, &master_encblock);
        (void) krb5_db_fini(context);
        return(1);
    }
    mblock.max_life = master_entry->max_life;
    mblock.max_rlife = master_entry->max_renewable_life;
    mblock.expiration = master_entry->expiration;
    /* don't set flags, master has some extra restrictions */
    mblock.mkvno = master_entry->key_data[0].key_data_kvno;

    krb5_db_free_principal(context, master_entry);
    free(args[0]);
    return 0;
}
