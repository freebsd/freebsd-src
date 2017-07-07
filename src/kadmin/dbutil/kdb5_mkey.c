/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <k5-int.h>
#include <kdb.h>
#include <kadm5/server_internal.h>
#include <kadm5/admin.h>
#include <adm_proto.h>
#include "kdb5_util.h"
#include <time.h>

#if defined(HAVE_COMPILE) && defined(HAVE_STEP)
#define SOLARIS_REGEXPS
#elif defined(HAVE_REGCOMP) && defined(HAVE_REGEXEC)
#define POSIX_REGEXPS
#elif defined(HAVE_RE_COMP) && defined(HAVE_RE_EXEC)
#define BSD_REGEXPS
#else
#error I cannot find any regexp functions
#endif
#ifdef SOLARIS_REGEXPS
#include        <regexpr.h>
#endif
#ifdef POSIX_REGEXPS
#include        <regex.h>
#endif

extern krb5_keyblock master_keyblock; /* current mkey */
extern krb5_kvno   master_kvno;
extern krb5_principal master_princ;
extern krb5_data master_salt;
extern char *mkey_fullname;
extern char *mkey_password;
extern char *progname;
extern int exit_status;
extern kadm5_config_params global_params;
extern krb5_context util_context;
extern time_t get_date(char *);

static char *strdate(krb5_timestamp when)
{
    struct tm *tm;
    static char out[40];

    time_t lcltim = when;
    tm = localtime(&lcltim);
    strftime(out, sizeof(out), "%a %b %d %H:%M:%S %Z %Y", tm);
    return out;
}

krb5_kvno
get_next_kvno(krb5_context context, krb5_db_entry *entry)
{
    krb5_kvno new_kvno;

    new_kvno = krb5_db_get_key_data_kvno(context, entry->n_key_data,
                                         entry->key_data);
    new_kvno++;
    /* deal with wrapping */
    if (new_kvno == 0)
        new_kvno = 1; /* knvo must not be 0 as this is special value (IGNORE_VNO) */

    return (new_kvno);
}

krb5_error_code
add_new_mkey(krb5_context context, krb5_db_entry *master_entry,
             krb5_keyblock *new_mkey, krb5_kvno use_mkvno)
{
    krb5_error_code retval = 0;
    int old_key_data_count, i;
    krb5_kvno new_mkey_kvno;
    krb5_key_data tmp_key_data;
    krb5_mkey_aux_node  *mkey_aux_data_head = NULL, **mkey_aux_data;
    krb5_keylist_node  *keylist_node;
    krb5_keylist_node *master_keylist = krb5_db_mkey_list_alias(context);

    /* do this before modifying master_entry key_data */
    new_mkey_kvno = get_next_kvno(context, master_entry);
    /* verify the requested mkvno if not 0 is the one that would be used here. */
    if (use_mkvno != 0 && new_mkey_kvno != use_mkvno)
        return (KRB5_KDB_KVNONOMATCH);

    old_key_data_count = master_entry->n_key_data;

    /* alloc enough space to hold new and existing key_data */
    /*
     * The encrypted key is malloc'ed by krb5_dbe_encrypt_key_data and
     * krb5_key_data key_data_contents is a pointer to this key.  Using some
     * logic from master_key_convert().
     */
    for (i = 0; i < master_entry->n_key_data; i++)
        krb5_free_key_data_contents(context, &master_entry->key_data[i]);
    free(master_entry->key_data);
    master_entry->key_data = (krb5_key_data *) malloc(sizeof(krb5_key_data) *
                                                      (old_key_data_count + 1));
    if (master_entry->key_data == NULL)
        return (ENOMEM);

    memset(master_entry->key_data, 0,
           sizeof(krb5_key_data) * (old_key_data_count + 1));
    master_entry->n_key_data = old_key_data_count + 1;

    /* Note, mkey does not have salt */
    /* add new mkey encrypted with itself to mkey princ entry */
    if ((retval = krb5_dbe_encrypt_key_data(context, new_mkey, new_mkey, NULL,
                                            (int) new_mkey_kvno,
                                            &master_entry->key_data[0]))) {
        return (retval);
    }
    /* the mvkno should be that of the newest mkey */
    if ((retval = krb5_dbe_update_mkvno(context, master_entry, new_mkey_kvno))) {
        krb5_free_key_data_contents(context, &master_entry->key_data[0]);
        return (retval);
    }
    /*
     * Need to decrypt old keys with the current mkey which is in the global
     * master_keyblock and encrypt those keys with the latest mkey.  And while
     * the old keys are being decrypted, use those to create the
     * KRB5_TL_MKEY_AUX entries which store the latest mkey encrypted by one of
     * the older mkeys.
     *
     * The new mkey is followed by existing keys.
     *
     * First, set up for creating a krb5_mkey_aux_node list which will be used
     * to update the mkey aux data for the mkey princ entry.
     */
    mkey_aux_data_head = (krb5_mkey_aux_node *) malloc(sizeof(krb5_mkey_aux_node));
    if (mkey_aux_data_head == NULL) {
        retval = ENOMEM;
        goto clean_n_exit;
    }
    memset(mkey_aux_data_head, 0, sizeof(krb5_mkey_aux_node));
    mkey_aux_data = &mkey_aux_data_head;

    for (keylist_node = master_keylist, i = 1; keylist_node != NULL;
         keylist_node = keylist_node->next, i++) {

        /*
         * Create a list of krb5_mkey_aux_node nodes.  One node contains the new
         * mkey encrypted by an old mkey and the old mkey's kvno (one node per
         * old mkey).
         */
        if (*mkey_aux_data == NULL) {
            /* *mkey_aux_data points to next field of previous node */
            *mkey_aux_data = (krb5_mkey_aux_node *) malloc(sizeof(krb5_mkey_aux_node));
            if (*mkey_aux_data == NULL) {
                retval = ENOMEM;
                goto clean_n_exit;
            }
            memset(*mkey_aux_data, 0, sizeof(krb5_mkey_aux_node));
        }

        memset(&tmp_key_data, 0, sizeof(tmp_key_data));
        /* encrypt the new mkey with the older mkey */
        retval = krb5_dbe_encrypt_key_data(context, &keylist_node->keyblock,
                                           new_mkey, NULL, (int) new_mkey_kvno,
                                           &tmp_key_data);
        if (retval)
            goto clean_n_exit;

        (*mkey_aux_data)->latest_mkey = tmp_key_data;
        (*mkey_aux_data)->mkey_kvno = keylist_node->kvno;
        mkey_aux_data = &((*mkey_aux_data)->next);

        /*
         * Store old key in master_entry keydata past the new mkey
         */
        retval = krb5_dbe_encrypt_key_data(context, new_mkey,
                                           &keylist_node->keyblock,
                                           NULL, (int) keylist_node->kvno,
                                           &master_entry->key_data[i]);
        if (retval)
            goto clean_n_exit;
    }
    assert(i == old_key_data_count + 1);

    if ((retval = krb5_dbe_update_mkey_aux(context, master_entry,
                                           mkey_aux_data_head))) {
        goto clean_n_exit;
    }
    master_entry->mask |= KADM5_KEY_DATA | KADM5_TL_DATA;

clean_n_exit:
    krb5_dbe_free_mkey_aux_list(context, mkey_aux_data_head);
    return (retval);
}

void
kdb5_add_mkey(int argc, char *argv[])
{
    int optchar;
    krb5_error_code retval;
    char *pw_str = 0;
    unsigned int pw_size = 0;
    int do_stash = 0;
    krb5_data pwd;
    krb5_kvno new_mkey_kvno;
    krb5_keyblock new_mkeyblock;
    krb5_enctype new_master_enctype = ENCTYPE_UNKNOWN;
    char *new_mkey_password;
    krb5_db_entry *master_entry = NULL;
    krb5_timestamp now;

    /*
     * The command table entry for this command causes open_db_and_mkey() to be
     * called first to open the KDB and get the current mkey.
     */

    memset(&new_mkeyblock, 0, sizeof(new_mkeyblock));
    master_salt.data = NULL;

    while ((optchar = getopt(argc, argv, "e:s")) != -1) {
        switch(optchar) {
        case 'e':
            if (krb5_string_to_enctype(optarg, &new_master_enctype)) {
                com_err(progname, EINVAL, _("%s is an invalid enctype"),
                        optarg);
                exit_status++;
                return;
            }
            break;
        case 's':
            do_stash++;
            break;
        case '?':
        default:
            usage();
            return;
        }
    }

    if (new_master_enctype == ENCTYPE_UNKNOWN)
        new_master_enctype = global_params.enctype;

    retval = krb5_db_get_principal(util_context, master_princ, 0,
                                   &master_entry);
    if (retval != 0) {
        com_err(progname, retval, _("while getting master key principal %s"),
                mkey_fullname);
        exit_status++;
        goto cleanup_return;
    }

    printf(_("Creating new master key for master key principal '%s'\n"),
           mkey_fullname);

    printf(_("You will be prompted for a new database Master Password.\n"));
    printf(_("It is important that you NOT FORGET this password.\n"));
    fflush(stdout);

    pw_size = 1024;
    pw_str = malloc(pw_size);
    if (pw_str == NULL) {
        com_err(progname, ENOMEM, _("while creating new master key"));
        exit_status++;
        goto cleanup_return;
    }

    retval = krb5_read_password(util_context, KRB5_KDC_MKEY_1, KRB5_KDC_MKEY_2,
                                pw_str, &pw_size);
    if (retval) {
        com_err(progname, retval,
                _("while reading new master key from keyboard"));
        exit_status++;
        goto cleanup_return;
    }
    new_mkey_password = pw_str;

    pwd.data = new_mkey_password;
    pwd.length = strlen(new_mkey_password);
    retval = krb5_principal2salt(util_context, master_princ, &master_salt);
    if (retval) {
        com_err(progname, retval, _("while calculating master key salt"));
        exit_status++;
        goto cleanup_return;
    }

    retval = krb5_c_string_to_key(util_context, new_master_enctype,
                                  &pwd, &master_salt, &new_mkeyblock);
    if (retval) {
        com_err(progname, retval,
                _("while transforming master key from password"));
        exit_status++;
        goto cleanup_return;
    }

    new_mkey_kvno = get_next_kvno(util_context, master_entry);
    retval = add_new_mkey(util_context, master_entry, &new_mkeyblock,
                          new_mkey_kvno);
    if (retval) {
        com_err(progname, retval,
                _("adding new master key to master principal"));
        exit_status++;
        goto cleanup_return;
    }

    if ((retval = krb5_timeofday(util_context, &now))) {
        com_err(progname, retval, _("while getting current time"));
        exit_status++;
        goto cleanup_return;
    }

    if ((retval = krb5_dbe_update_mod_princ_data(util_context, master_entry,
                                                 now, master_princ))) {
        com_err(progname, retval, _("while updating the master key principal "
                                    "modification time"));
        exit_status++;
        goto cleanup_return;
    }

    if ((retval = krb5_db_put_principal(util_context, master_entry))) {
        com_err(progname, retval, _("while adding master key entry to the "
                                    "database"));
        exit_status++;
        goto cleanup_return;
    }

    if (do_stash) {
        retval = krb5_db_store_master_key(util_context,
                                          global_params.stash_file,
                                          master_princ,
                                          new_mkey_kvno,
                                          &new_mkeyblock,
                                          mkey_password);
        if (retval) {
            com_err(progname, retval, _("while storing key"));
            printf(_("Warning: couldn't stash master key.\n"));
        }
    }

cleanup_return:
    /* clean up */
    krb5_db_free_principal(util_context, master_entry);
    zap((char *)new_mkeyblock.contents, new_mkeyblock.length);
    free(new_mkeyblock.contents);
    if (pw_str) {
        zap(pw_str, pw_size);
        free(pw_str);
    }
    free(master_salt.data);
    return;
}

void
kdb5_use_mkey(int argc, char *argv[])
{
    krb5_error_code retval;
    krb5_kvno  use_kvno;
    krb5_timestamp now, start_time;
    krb5_actkvno_node *actkvno_list = NULL, *new_actkvno = NULL,
        *prev_actkvno, *cur_actkvno;
    krb5_db_entry *master_entry = NULL;
    krb5_keylist_node *keylist_node;
    krb5_boolean inserted = FALSE;
    krb5_keylist_node *master_keylist = krb5_db_mkey_list_alias(util_context);

    if (argc < 2 || argc > 3) {
        /* usage calls exit */
        usage();
    }

    use_kvno = atoi(argv[1]);
    if (use_kvno == 0) {
        com_err(progname, EINVAL, _("0 is an invalid KVNO value"));
        exit_status++;
        return;
    } else {
        /* verify use_kvno is valid */
        for (keylist_node = master_keylist; keylist_node != NULL;
             keylist_node = keylist_node->next) {
            if (use_kvno == keylist_node->kvno)
                break;
        }
        if (!keylist_node) {
            com_err(progname, EINVAL, _("%d is an invalid KVNO value"),
                    use_kvno);
            exit_status++;
            return;
        }
    }

    if ((retval = krb5_timeofday(util_context, &now))) {
        com_err(progname, retval, _("while getting current time"));
        exit_status++;
        return;
    }

    if (argc == 3) {
        time_t t = get_date(argv[2]);
        if (t == -1) {
            com_err(progname, 0, _("could not parse date-time string '%s'"),
                    argv[2]);
            exit_status++;
            return;
        } else
            start_time = (krb5_timestamp) t;
    } else {
        start_time = now;
    }

    /*
     * Need to:
     *
     * 1. get mkey princ
     * 2. get krb5_actkvno_node list
     * 3. add use_kvno to actkvno list (sorted in right spot)
     * 4. update mkey princ's tl data
     * 5. put mkey princ.
     */

    retval = krb5_db_get_principal(util_context, master_princ, 0,
                                   &master_entry);
    if (retval != 0) {
        com_err(progname, retval, _("while getting master key principal %s"),
                mkey_fullname);
        exit_status++;
        goto cleanup_return;
    }

    retval = krb5_dbe_lookup_actkvno(util_context, master_entry, &actkvno_list);
    if (retval != 0) {
        com_err(progname, retval,
                _("while looking up active version of master key"));
        exit_status++;
        goto cleanup_return;
    }

    /*
     * If an entry already exists with the same kvno either delete it or if it's
     * the only entry, just set its active time.
     */
    for (prev_actkvno = NULL, cur_actkvno = actkvno_list;
         cur_actkvno != NULL;
         prev_actkvno = cur_actkvno, cur_actkvno = cur_actkvno->next) {

        if (cur_actkvno->act_kvno == use_kvno) {
            /* delete it */
            if (prev_actkvno) {
                prev_actkvno->next = cur_actkvno->next;
                cur_actkvno->next = NULL;
                krb5_dbe_free_actkvno_list(util_context, cur_actkvno);
            } else {
                if (cur_actkvno->next) {
                    /* delete it from front of list */
                    actkvno_list = cur_actkvno->next;
                    cur_actkvno->next = NULL;
                    krb5_dbe_free_actkvno_list(util_context, cur_actkvno);
                } else {
                    /* There's only one entry, go ahead and change the time */
                    cur_actkvno->act_time = start_time;
                    inserted = TRUE;
                }
            }
            break;
        }
    }

    if (!inserted) {
        /* alloc enough space to hold new and existing key_data */
        new_actkvno = (krb5_actkvno_node *) malloc(sizeof(krb5_actkvno_node));
        if (new_actkvno == NULL) {
            com_err(progname, ENOMEM, _("while adding new master key"));
            exit_status++;
            goto cleanup_return;
        }
        memset(new_actkvno, 0, sizeof(krb5_actkvno_node));
        new_actkvno->act_kvno = use_kvno;
        new_actkvno->act_time = start_time;

        /* insert new act kvno node */

        if (actkvno_list == NULL) {
            /* new actkvno is the list */
            actkvno_list = new_actkvno;
        } else {
            for (prev_actkvno = NULL, cur_actkvno = actkvno_list;
                 cur_actkvno != NULL;
                 prev_actkvno = cur_actkvno, cur_actkvno = cur_actkvno->next) {

                if (new_actkvno->act_time < cur_actkvno->act_time) {
                    if (prev_actkvno) {
                        prev_actkvno->next = new_actkvno;
                        new_actkvno->next = cur_actkvno;
                    } else {
                        new_actkvno->next = actkvno_list;
                        actkvno_list = new_actkvno;
                    }
                    break;
                } else if (cur_actkvno->next == NULL) {
                    /* end of line, just add new node to end of list */
                    cur_actkvno->next = new_actkvno;
                    break;
                }
            }
        }
    }

    if (actkvno_list->act_time > now) {
        com_err(progname, EINVAL,
                _("there must be one master key currently active"));
        exit_status++;
        goto cleanup_return;
    }

    if ((retval = krb5_dbe_update_actkvno(util_context, master_entry,
                                          actkvno_list))) {
        com_err(progname, retval,
                _("while updating actkvno data for master principal entry"));
        exit_status++;
        goto cleanup_return;
    }

    if ((retval = krb5_dbe_update_mod_princ_data(util_context, master_entry,
                                                 now, master_princ))) {
        com_err(progname, retval, _("while updating the master key principal "
                                    "modification time"));
        exit_status++;
        goto cleanup_return;
    }

    if ((retval = krb5_db_put_principal(util_context, master_entry))) {
        com_err(progname, retval,
                _("while adding master key entry to the database"));
        exit_status++;
        goto cleanup_return;
    }

cleanup_return:
    /* clean up */
    krb5_db_free_principal(util_context, master_entry);
    krb5_dbe_free_actkvno_list(util_context, actkvno_list);
    return;
}

void
kdb5_list_mkeys(int argc, char *argv[])
{
    krb5_error_code retval;
    char *output_str = NULL, enctype[BUFSIZ];
    krb5_kvno  act_kvno;
    krb5_timestamp act_time;
    krb5_actkvno_node *actkvno_list = NULL, *cur_actkvno;
    krb5_db_entry *master_entry = NULL;
    krb5_keylist_node  *cur_kb_node;
    krb5_keyblock *act_mkey;
    krb5_keylist_node *master_keylist = krb5_db_mkey_list_alias(util_context);

    if (master_keylist == NULL) {
        com_err(progname, 0, _("master keylist not initialized"));
        exit_status++;
        return;
    }

    retval = krb5_db_get_principal(util_context, master_princ, 0,
                                   &master_entry);
    if (retval != 0) {
        com_err(progname, retval, _("while getting master key principal %s"),
                mkey_fullname);
        exit_status++;
        goto cleanup_return;
    }

    retval = krb5_dbe_lookup_actkvno(util_context, master_entry, &actkvno_list);
    if (retval != 0) {
        com_err(progname, retval, _("while looking up active kvno list"));
        exit_status++;
        goto cleanup_return;
    }

    retval = krb5_dbe_find_act_mkey(util_context, actkvno_list, &act_kvno,
                                    &act_mkey);
    if (retval != 0) {
        com_err(progname, retval, _("while looking up active master key"));
        exit_status++;
        goto cleanup_return;
    }

    printf("Master keys for Principal: %s\n", mkey_fullname);

    for (cur_kb_node = master_keylist; cur_kb_node != NULL;
         cur_kb_node = cur_kb_node->next) {

        if ((retval = krb5_enctype_to_name(cur_kb_node->keyblock.enctype,
                                           FALSE, enctype, sizeof(enctype)))) {
            com_err(progname, retval, _("while getting enctype description"));
            exit_status++;
            goto cleanup_return;
        }

        act_time = -1; /* assume actkvno entry not found */
        for (cur_actkvno = actkvno_list; cur_actkvno != NULL;
             cur_actkvno = cur_actkvno->next) {
            if (cur_actkvno->act_kvno == cur_kb_node->kvno) {
                act_time = cur_actkvno->act_time;
                break;
            }
        }

        if (cur_kb_node->kvno == act_kvno) {
            /* * indicates kvno is currently active */
            retval = asprintf(&output_str,
                              _("KVNO: %d, Enctype: %s, Active on: %s *\n"),
                              cur_kb_node->kvno, enctype, strdate(act_time));
        } else {
            if (act_time != -1) {
                retval = asprintf(&output_str,
                                  _("KVNO: %d, Enctype: %s, Active on: %s\n"),
                                  cur_kb_node->kvno, enctype, strdate(act_time));
            } else {
                retval = asprintf(&output_str,
                                  _("KVNO: %d, Enctype: %s, No activate time "
                                    "set\n"), cur_kb_node->kvno, enctype);
            }
        }
        if (retval == -1) {
            com_err(progname, ENOMEM, _("asprintf could not allocate enough "
                                        "memory to hold output"));
            exit_status++;
            goto cleanup_return;
        }
        printf("%s", output_str);
        free(output_str);
        output_str = NULL;
    }

cleanup_return:
    /* clean up */
    krb5_db_free_principal(util_context, master_entry);
    free(output_str);
    krb5_dbe_free_actkvno_list(util_context, actkvno_list);
    return;
}

struct update_enc_mkvno {
    unsigned int re_match_count;
    unsigned int already_current;
    unsigned int updated;
    unsigned int dry_run : 1;
    unsigned int verbose : 1;
#ifdef SOLARIS_REGEXPS
    char *expbuf;
#endif
#ifdef POSIX_REGEXPS
    regex_t preg;
#endif
#if !defined(SOLARIS_REGEXPS) && !defined(POSIX_REGEXPS)
    unsigned char placeholder;
#endif
};

/* XXX Duplicated in libkadm5srv! */
/*
 * Function: glob_to_regexp
 *
 * Arguments:
 *
 *      glob    (r) the shell-style glob (?*[]) to convert
 *      realm   (r) the default realm to append, or NULL
 *      regexp  (w) the ed-style regexp created from glob
 *
 * Effects:
 *
 * regexp is filled in with allocated memory contained a regular
 * expression to be used with re_comp/compile that matches what the
 * shell-style glob would match.  If glob does not contain an "@"
 * character and realm is not NULL, "@*" is appended to the regexp.
 *
 * Conversion algorithm:
 *
 *      quoted characters are copied quoted
 *      ? is converted to .
 *      * is converted to .*
 *      active characters are quoted: ^, $, .
 *      [ and ] are active but supported and have the same meaning, so
 *              they are copied
 *      other characters are copied
 *      regexp is anchored with ^ and $
 */
static int glob_to_regexp(char *glob, char *realm, char **regexp)
{
    int append_realm;
    char *p;

    /* validate the glob */
    if (glob[strlen(glob)-1] == '\\')
        return EINVAL;

    /* A character of glob can turn into two in regexp, plus ^ and $ */
    /* and trailing null.  If glob has no @, also allocate space for */
    /* the realm. */
    append_realm = (realm != NULL) && (strchr(glob, '@') == NULL);
    p = (char *) malloc(strlen(glob)*2+ 3 + (append_realm ? 3 : 0));
    if (p == NULL)
        return ENOMEM;
    *regexp = p;

    *p++ = '^';
    while (*glob) {
        switch (*glob) {
        case '?':
            *p++ = '.';
            break;
        case '*':
            *p++ = '.';
            *p++ = '*';
            break;
        case '.':
        case '^':
        case '$':
            *p++ = '\\';
            *p++ = *glob;
            break;
        case '\\':
            *p++ = '\\';
            *p++ = *++glob;
            break;
        default:
            *p++ = *glob;
            break;
        }
        glob++;
    }

    if (append_realm) {
        *p++ = '@';
        *p++ = '.';
        *p++ = '*';
    }

    *p++ = '$';
    *p++ = '\0';
    return 0;
}

static int
update_princ_encryption_1(void *cb, krb5_db_entry *ent)
{
    struct update_enc_mkvno *p = cb;
    char *pname = 0;
    krb5_error_code retval;
    int match;
    krb5_timestamp now;
    int result;
    krb5_kvno old_mkvno;

    retval = krb5_unparse_name(util_context, ent->princ, &pname);
    if (retval) {
        com_err(progname, retval,
                _("getting string representation of principal name"));
        goto fail;
    }

    if (krb5_principal_compare(util_context, ent->princ, master_princ)) {
        goto skip;
    }

#ifdef SOLARIS_REGEXPS
    match = (step(pname, p->expbuf) != 0);
#endif
#ifdef POSIX_REGEXPS
    match = (regexec(&p->preg, pname, 0, NULL, 0) == 0);
#endif
#ifdef BSD_REGEXPS
    match = (re_exec(pname) != 0);
#endif
    if (!match) {
        goto skip;
    }
    p->re_match_count++;
    retval = krb5_dbe_get_mkvno(util_context, ent, &old_mkvno);
    if (retval) {
        com_err(progname, retval,
                _("determining master key used for principal '%s'"), pname);
        goto fail;
    }
    /* Line up "skip" and "update" messages for viewing.  */
    if (old_mkvno == new_mkvno) {
        if (p->dry_run && p->verbose)
            printf(_("would skip:   %s\n"), pname);
        else if (p->verbose)
            printf(_("skipping: %s\n"), pname);
        p->already_current++;
        goto skip;
    }
    if (p->dry_run) {
        if (p->verbose)
            printf(_("would update: %s\n"), pname);
        p->updated++;
        goto skip;
    } else if (p->verbose)
        printf(_("updating: %s\n"), pname);
    retval = master_key_convert (util_context, ent);
    if (retval) {
        com_err(progname, retval,
                _("error re-encrypting key for principal '%s'"), pname);
        goto fail;
    }
    if ((retval = krb5_timeofday(util_context, &now))) {
        com_err(progname, retval, _("while getting current time"));
        goto fail;
    }

    if ((retval = krb5_dbe_update_mod_princ_data(util_context, ent,
                                                 now, master_princ))) {
        com_err(progname, retval,
                _("while updating principal '%s' modification time"), pname);
        goto fail;
    }

    ent->mask |= KADM5_KEY_DATA;

    if ((retval = krb5_db_put_principal(util_context, ent))) {
        com_err(progname, retval, _("while updating principal '%s' key data "
                                    "in the database"), pname);
        goto fail;
    }
    p->updated++;
skip:
    result = 0;
    goto egress;
fail:
    exit_status++;
    result = 1;
egress:
    if (pname)
        krb5_free_unparsed_name(util_context, pname);
    return result;
}

extern int are_you_sure (const char *, ...)
#if !defined(__cplusplus) && (__GNUC__ > 2)
    __attribute__((__format__(__printf__, 1, 2)))
#endif
    ;

int
are_you_sure (const char *format, ...)
{
    va_list va;
    char ansbuf[100];

    va_start(va, format);
    vprintf(format, va);
    va_end(va);
    printf(_("\n(type 'yes' to confirm)? "));
    fflush(stdout);
    if (fgets(ansbuf, sizeof(ansbuf), stdin) == NULL)
        return 0;
    if (strcmp(ansbuf, "yes\n"))
        return 0;
    return 1;
}

void
kdb5_update_princ_encryption(int argc, char *argv[])
{
    struct update_enc_mkvno data = { 0 };
    char *name_pattern = NULL;
    int force = 0;
    int optchar;
    krb5_error_code retval;
    krb5_actkvno_node *actkvno_list = 0;
    krb5_db_entry *master_entry = NULL;
#ifdef BSD_REGEXPS
    char *msg;
#endif
    char *regexp = NULL;
    krb5_keyblock *act_mkey;
    krb5_keylist_node *master_keylist = krb5_db_mkey_list_alias(util_context);
    krb5_flags iterflags = 0;

    while ((optchar = getopt(argc, argv, "fnv")) != -1) {
        switch (optchar) {
        case 'f':
            force = 1;
            break;
        case 'n':
            data.dry_run = 1;
            break;
        case 'v':
            data.verbose = 1;
            break;
        case '?':
        case ':':
        default:
            usage();
        }
    }
    if (argv[optind] != NULL) {
        name_pattern = argv[optind];
        if (argv[optind+1] != NULL)
            usage();
    }

    if (master_keylist == NULL) {
        com_err(progname, 0, _("master keylist not initialized"));
        exit_status++;
        goto cleanup;
    }

    /* The glob_to_regexp code only cares if the "realm" parameter is
       NULL or not; the string data is irrelevant.  */
    if (name_pattern == NULL)
        name_pattern = "*";
    if (glob_to_regexp(name_pattern, "hi", &regexp) != 0) {
        com_err(progname, ENOMEM,
                _("converting glob pattern '%s' to regular expression"),
                name_pattern);
        exit_status++;
        goto cleanup;
    }

    if (
#ifdef SOLARIS_REGEXPS
        ((data.expbuf = compile(regexp, NULL, NULL)) == NULL)
#endif
#ifdef POSIX_REGEXPS
        ((regcomp(&data.preg, regexp, REG_NOSUB)) != 0)
#endif
#ifdef BSD_REGEXPS
        ((msg = (char *) re_comp(regexp)) != NULL)
#endif
    ) {
        /* XXX syslog msg or regerr(regerrno) */
        com_err(progname, 0, _("error compiling converted regexp '%s'"),
                regexp);
        exit_status++;
        goto cleanup;
    }

    retval = krb5_db_get_principal(util_context, master_princ, 0,
                                   &master_entry);
    if (retval != 0) {
        com_err(progname, retval, _("while getting master key principal %s"),
                mkey_fullname);
        exit_status++;
        goto cleanup;
    }

    retval = krb5_dbe_lookup_actkvno(util_context, master_entry, &actkvno_list);
    if (retval != 0) {
        com_err(progname, retval, _("while looking up active kvno list"));
        exit_status++;
        goto cleanup;
    }

    retval = krb5_dbe_find_act_mkey(util_context, actkvno_list, &new_mkvno,
                                    &act_mkey);
    if (retval) {
        com_err(progname, retval, _("while looking up active master key"));
        exit_status++;
        goto cleanup;
    }
    new_master_keyblock = *act_mkey;

    if (!force &&
        !data.dry_run &&
        !are_you_sure(_("Re-encrypt all keys not using master key vno %u?"),
                      new_mkvno)) {
        printf(_("OK, doing nothing.\n"));
        exit_status++;
        goto cleanup;
    }
    if (data.verbose) {
        if (data.dry_run) {
            printf(_("Principals whose keys WOULD BE re-encrypted to master "
                     "key vno %u:\n"), new_mkvno);
        } else {
            printf(_("Principals whose keys are being re-encrypted to master "
                     "key vno %u if necessary:\n"), new_mkvno);
        }
    }

    if (!data.dry_run) {
        /* Grab a write lock so we don't have to upgrade to a write lock and
         * reopen the DB while iterating. */
        iterflags = KRB5_DB_ITER_WRITE;
    }

    retval = krb5_db_iterate(util_context, name_pattern,
                             update_princ_encryption_1, &data, iterflags);
    /* If exit_status is set, then update_princ_encryption_1 already
       printed a message.  */
    if (retval != 0 && exit_status == 0) {
        com_err(progname, retval, _("trying to process principal database"));
        exit_status++;
    }
    if (data.dry_run) {
        printf(_("%u principals processed: %u would be updated, %u already "
                 "current\n"),
               data.re_match_count, data.updated, data.already_current);
    } else {
        printf(_("%u principals processed: %u updated, %u already current\n"),
               data.re_match_count, data.updated, data.already_current);
    }

cleanup:
    krb5_db_free_principal(util_context, master_entry);
    free(regexp);
#ifdef POSIX_REGEXPS
    regfree(&data.preg);
#endif
    memset(&new_master_keyblock, 0, sizeof(new_master_keyblock));
    krb5_dbe_free_actkvno_list(util_context, actkvno_list);
}

struct kvnos_in_use {
    krb5_kvno               kvno;
    unsigned int            use_count;
};

struct purge_args {
    krb5_context         kcontext;
    struct kvnos_in_use  *kvnos;
    unsigned int         num_kvnos;
};

static krb5_error_code
find_mkvnos_in_use(krb5_pointer   ptr,
                   krb5_db_entry *entry)
{
    krb5_error_code retval;
    struct purge_args * args;
    unsigned int i;
    krb5_kvno mkvno;

    args = (struct purge_args *) ptr;

    retval = krb5_dbe_get_mkvno(args->kcontext, entry, &mkvno);
    if (retval)
        return (retval);

    for (i = 0; i < args->num_kvnos; i++) {
        if (args->kvnos[i].kvno == mkvno) {
            /* XXX do I need to worry about use_count wrapping? */
            args->kvnos[i].use_count++;
            break;
        }
    }
    return 0;
}

void
kdb5_purge_mkeys(int argc, char *argv[])
{
    int optchar;
    krb5_error_code retval;
    krb5_timestamp now;
    krb5_db_entry *master_entry = NULL;
    krb5_boolean force = FALSE, dry_run = FALSE, verbose = FALSE;
    struct purge_args args;
    char buf[5];
    unsigned int i, j, k, num_kvnos_inuse, num_kvnos_purged;
    unsigned int old_key_data_count;
    krb5_actkvno_node *actkvno_list = NULL, *actkvno_entry, *prev_actkvno_entry;
    krb5_mkey_aux_node *mkey_aux_list = NULL, *mkey_aux_entry, *prev_mkey_aux_entry;
    krb5_key_data *old_key_data;

    /*
     * Verify that the master key list has been initialized before doing
     * anything else.
     */
    if (krb5_db_mkey_list_alias(util_context) == NULL) {
        com_err(progname, KRB5_KDB_DBNOTINITED,
                _("master keylist not initialized"));
        exit_status++;
        return;
    }

    memset(&args, 0, sizeof(args));

    optind = 1;
    while ((optchar = getopt(argc, argv, "fnv")) != -1) {
        switch(optchar) {
        case 'f':
            force = TRUE;
            break;
        case 'n':
            dry_run = TRUE; /* mkey princ will not be modified */
            force = TRUE; /* implied */
            break;
        case 'v':
            verbose = TRUE;
            break;
        case '?':
        default:
            usage();
            return;
        }
    }

    retval = krb5_db_get_principal(util_context, master_princ, 0,
                                   &master_entry);
    if (retval != 0) {
        com_err(progname, retval, _("while getting master key principal %s"),
                mkey_fullname);
        exit_status++;
        goto cleanup_return;
    }

    if (!force) {
        printf(_("Will purge all unused master keys stored in the '%s' "
                 "principal, are you sure?\n"), mkey_fullname);
        printf(_("(type 'yes' to confirm)? "));
        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            exit_status++;
            goto cleanup_return;
        }
        if (strcmp(buf, "yes\n")) {
            exit_status++;
            goto cleanup_return;
        }
        printf(_("OK, purging unused master keys from '%s'...\n"),
               mkey_fullname);
    }

    /* save the old keydata */
    old_key_data_count = master_entry->n_key_data;
    if (old_key_data_count == 1) {
        if (verbose)
            printf(_("There is only one master key which can not be "
                     "purged.\n"));
        goto cleanup_return;
    }
    old_key_data = master_entry->key_data;

    args.kvnos = (struct kvnos_in_use *) malloc(sizeof(struct kvnos_in_use) * old_key_data_count);
    if (args.kvnos == NULL) {
        retval = ENOMEM;
        com_err(progname, ENOMEM, _("while allocating args.kvnos"));
        exit_status++;
        goto cleanup_return;
    }
    memset(args.kvnos, 0, sizeof(struct kvnos_in_use) * old_key_data_count);
    args.num_kvnos = old_key_data_count;
    args.kcontext = util_context;

    /* populate the kvnos array with all the current mkvnos */
    for (i = 0; i < old_key_data_count; i++)
        args.kvnos[i].kvno =  master_entry->key_data[i].key_data_kvno;

    if ((retval = krb5_db_iterate(util_context,
                                  NULL,
                                  find_mkvnos_in_use,
                                  (krb5_pointer) &args, 0))) {
        com_err(progname, retval, _("while finding master keys in use"));
        exit_status++;
        goto cleanup_return;
    }
    /*
     * args.kvnos has been marked with the mkvno's that are currently protecting
     * princ entries
     */
    if (dry_run) {
        printf(_("Would purge the following master key(s) from %s:\n"),
               mkey_fullname);
    } else {
        printf(_("Purging the following master key(s) from %s:\n"),
               mkey_fullname);
    }

    /* find # of keys still in use or print out verbose info */
    for (i = num_kvnos_inuse = num_kvnos_purged = 0; i < args.num_kvnos; i++) {
        if (args.kvnos[i].use_count > 0) {
            num_kvnos_inuse++;
        } else {
            /* this key would be deleted */
            if (args.kvnos[i].kvno == master_kvno) {
                com_err(progname, KRB5_KDB_STORED_MKEY_NOTCURRENT,
                        _("master key stash file needs updating, command "
                          "aborting"));
                exit_status++;
                goto cleanup_return;
            }
            num_kvnos_purged++;
            printf(_("KVNO: %d\n"), args.kvnos[i].kvno);
        }
    }
    /* didn't find any keys to purge */
    if (num_kvnos_inuse == args.num_kvnos) {
        printf(_("All keys in use, nothing purged.\n"));
        goto cleanup_return;
    }
    if (dry_run) {
        /* bail before doing anything else */
        printf(_("%d key(s) would be purged.\n"), num_kvnos_purged);
        goto cleanup_return;
    }

    retval = krb5_dbe_lookup_actkvno(util_context, master_entry, &actkvno_list);
    if (retval != 0) {
        com_err(progname, retval, _("while looking up active kvno list"));
        exit_status++;
        goto cleanup_return;
    }

    retval = krb5_dbe_lookup_mkey_aux(util_context, master_entry, &mkey_aux_list);
    if (retval != 0) {
        com_err(progname, retval, _("while looking up mkey aux data list"));
        exit_status++;
        goto cleanup_return;
    }

    master_entry->key_data = (krb5_key_data *) malloc(sizeof(krb5_key_data) * num_kvnos_inuse);
    if (master_entry->key_data == NULL) {
        retval = ENOMEM;
        com_err(progname, ENOMEM, _("while allocating key_data"));
        exit_status++;
        goto cleanup_return;
    }
    memset(master_entry->key_data, 0, sizeof(krb5_key_data) * num_kvnos_inuse);
    master_entry->n_key_data = num_kvnos_inuse; /* there's only 1 mkey per kvno */

    /*
     * Assuming that the latest mkey will not be purged because it will always
     * be "in use" so this code will not bother with encrypting keys again.
     */
    for (i = k = 0; i < old_key_data_count; i++) {
        for (j = 0; j < args.num_kvnos; j++) {
            if (args.kvnos[j].kvno == (krb5_kvno) old_key_data[i].key_data_kvno) {
                if (args.kvnos[j].use_count != 0) {
                    master_entry->key_data[k++] = old_key_data[i];
                    memset(&old_key_data[i], 0, sizeof(old_key_data[i]));
                    break;
                } else {
                    /* remove unused mkey */
                    /* adjust the actkno data */
                    for (prev_actkvno_entry = actkvno_entry = actkvno_list;
                         actkvno_entry != NULL;
                         actkvno_entry = actkvno_entry->next) {

                        if (actkvno_entry->act_kvno == args.kvnos[j].kvno) {
                            if (actkvno_entry == actkvno_list) {
                                /* remove from head */
                                actkvno_list = actkvno_entry->next;
                                prev_actkvno_entry = actkvno_list;
                            } else if (actkvno_entry->next == NULL) {
                                /* remove from tail */
                                prev_actkvno_entry->next = NULL;
                            } else {
                                /* remove in between */
                                prev_actkvno_entry->next = actkvno_entry->next;
                            }
                            actkvno_entry->next = NULL;
                            krb5_dbe_free_actkvno_list(util_context, actkvno_entry);
                            break; /* deleted entry, no need to loop further */
                        } else {
                            prev_actkvno_entry = actkvno_entry;
                        }
                    }
                    /* adjust the mkey aux data */
                    for (prev_mkey_aux_entry = mkey_aux_entry = mkey_aux_list;
                         mkey_aux_entry != NULL;
                         mkey_aux_entry = mkey_aux_entry->next) {

                        if (mkey_aux_entry->mkey_kvno == args.kvnos[j].kvno) {
                            if (mkey_aux_entry == mkey_aux_list) {
                                mkey_aux_list = mkey_aux_entry->next;
                                prev_mkey_aux_entry = mkey_aux_list;
                            } else if (mkey_aux_entry->next == NULL) {
                                prev_mkey_aux_entry->next = NULL;
                            } else {
                                prev_mkey_aux_entry->next = mkey_aux_entry->next;
                            }
                            mkey_aux_entry->next = NULL;
                            krb5_dbe_free_mkey_aux_list(util_context, mkey_aux_entry);
                            break; /* deleted entry, no need to loop further */
                        } else {
                            prev_mkey_aux_entry = mkey_aux_entry;
                        }
                    }
                }
            }
        }
    }
    assert(k == num_kvnos_inuse);

    /* Free any key data entries we did not consume in the loop above. */
    for (i = 0; i < old_key_data_count; i++)
        krb5_dbe_free_key_data_contents(util_context, &old_key_data[i]);
    free(old_key_data);

    if ((retval = krb5_dbe_update_actkvno(util_context, master_entry,
                                          actkvno_list))) {
        com_err(progname, retval,
                _("while updating actkvno data for master principal entry"));
        exit_status++;
        goto cleanup_return;
    }

    if ((retval = krb5_dbe_update_mkey_aux(util_context, master_entry,
                                           mkey_aux_list))) {
        com_err(progname, retval,
                _("while updating mkey_aux data for master principal entry"));
        exit_status++;
        return;
    }

    if ((retval = krb5_timeofday(util_context, &now))) {
        com_err(progname, retval, _("while getting current time"));
        exit_status++;
        goto cleanup_return;
    }

    if ((retval = krb5_dbe_update_mod_princ_data(util_context, master_entry,
                                                 now, master_princ))) {
        com_err(progname, retval, _("while updating the master key principal "
                                    "modification time"));
        exit_status++;
        goto cleanup_return;
    }

    master_entry->mask |= KADM5_KEY_DATA | KADM5_TL_DATA;

    if ((retval = krb5_db_put_principal(util_context, master_entry))) {
        com_err(progname, retval,
                _("while adding master key entry to the database"));
        exit_status++;
        goto cleanup_return;
    }
    printf(_("%d key(s) purged.\n"), num_kvnos_purged);

cleanup_return:
    krb5_db_free_principal(util_context, master_entry);
    free(args.kvnos);
    krb5_dbe_free_actkvno_list(util_context, actkvno_list);
    krb5_dbe_free_mkey_aux_list(util_context, mkey_aux_list);
    return;
}
