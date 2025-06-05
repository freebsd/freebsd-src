/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kadmin/ktutil/ktutil_funcs.c */
/*
 *(C) Copyright 1995, 1996 by the Massachusetts Institute of Technology.
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
 * Utility functions for ktutil.
 */

#include "k5-int.h"
#include "k5-hex.h"
#include "ktutil.h"
#include <string.h>
#include <ctype.h>

/*
 * Free a kt_list
 */
krb5_error_code ktutil_free_kt_list(context, list)
    krb5_context context;
    krb5_kt_list list;
{
    krb5_kt_list lp, prev;
    krb5_error_code retval = 0;

    for (lp = list; lp;) {
        retval = krb5_kt_free_entry(context, lp->entry);
        free(lp->entry);
        if (retval)
            break;
        prev = lp;
        lp = lp->next;
        free(prev);
    }
    return retval;
}

/*
 * Delete a numbered entry in a kt_list.  Takes a pointer to a kt_list
 * in case head gets deleted.
 */
krb5_error_code ktutil_delete(context, list, idx)
    krb5_context context;
    krb5_kt_list *list;
    int idx;
{
    krb5_kt_list lp, prev;
    int i;

    for (lp = *list, i = 1; lp; prev = lp, lp = lp->next, i++) {
        if (i == idx) {
            if (i == 1)
                *list = lp->next;
            else
                prev->next = lp->next;
            lp->next = NULL;
            return ktutil_free_kt_list(context, lp);
        }
    }
    return EINVAL;
}

/*
 * Determine the enctype, salt, and s2kparams for princ based on the presence
 * of the -f flag (fetch), the optionally specified salt string, and the
 * optionally specified enctype.  If the fetch flag is used, salt_str must not
 * be given; if the fetch flag is not used, the enctype must be given.
 */
static krb5_error_code
get_etype_info(krb5_context context, krb5_principal princ, int fetch,
               char *salt_str, krb5_enctype *enctype_inout,
               krb5_data *salt_out, krb5_data *s2kparams_out)
{
    krb5_error_code retval;
    krb5_enctype enctype;
    krb5_get_init_creds_opt *opt = NULL;
    krb5_data salt;

    *salt_out = empty_data();
    *s2kparams_out = empty_data();

    if (!fetch) {
        /* Use the specified enctype and either the specified or default salt.
         * Do not produce s2kparams. */
        assert(*enctype_inout != ENCTYPE_NULL);
        if (salt_str != NULL) {
            salt = string2data(salt_str);
            return krb5int_copy_data_contents(context, &salt, salt_out);
        } else {
            return krb5_principal2salt(context, princ, salt_out);
        }
    }

    /* Get etype-info from the KDC. */
    assert(salt_str == NULL);
    if (*enctype_inout != ENCTYPE_NULL) {
        retval = krb5_get_init_creds_opt_alloc(context, &opt);
        if (retval)
            return retval;
        krb5_get_init_creds_opt_set_etype_list(opt, enctype_inout, 1);
    }
    retval = krb5_get_etype_info(context, princ, opt, &enctype, salt_out,
                                 s2kparams_out);
    krb5_get_init_creds_opt_free(context, opt);
    if (retval)
        return retval;
    if (enctype == ENCTYPE_NULL)
        return KRB5KDC_ERR_ETYPE_NOSUPP;

    *enctype_inout = enctype;
    return 0;
}

/*
 * Create a new keytab entry and add it to the keytab list.
 * Based on the value of use_pass, either prompt the user for a
 * password or key.  If the keytab list is NULL, allocate a new
 * one first.
 */
krb5_error_code ktutil_add(context, list, princ_str, fetch, kvno,
                           enctype_str, use_pass, salt_str)
    krb5_context context;
    krb5_kt_list *list;
    char *princ_str;
    int fetch;
    krb5_kvno kvno;
    char *enctype_str;
    int use_pass;
    char *salt_str;
{
    krb5_keytab_entry *entry = NULL;
    krb5_kt_list lp, *last;
    krb5_principal princ;
    krb5_enctype enctype = ENCTYPE_NULL;
    krb5_timestamp now;
    krb5_error_code retval;
    krb5_data password = empty_data(), salt = empty_data();
    krb5_data params = empty_data(), *s2kparams;
    krb5_keyblock key;
    char buf[BUFSIZ];
    char promptstr[1024];
    char *princ_full = NULL;
    uint8_t *keybytes;
    size_t keylen;
    unsigned int pwsize = BUFSIZ;

    retval = krb5_parse_name(context, princ_str, &princ);
    if (retval)
        goto cleanup;
    /* now unparse in order to get the default realm appended
       to princ_str, if no realm was specified */
    retval = krb5_unparse_name(context, princ, &princ_full);
    if (retval)
        goto cleanup;
    if (enctype_str != NULL) {
        retval = krb5_string_to_enctype(enctype_str, &enctype);
        if (retval) {
            retval = KRB5_BAD_ENCTYPE;
            goto cleanup;
        }
    }
    retval = krb5_timeofday(context, &now);
    if (retval)
        goto cleanup;

    entry = k5alloc(sizeof(*entry), &retval);
    if (entry == NULL)
        goto cleanup;

    if (use_pass) {
        retval = alloc_data(&password, pwsize);
        if (retval)
            goto cleanup;

        snprintf(promptstr, sizeof(promptstr), _("Password for %.1000s"),
                 princ_full);
        retval = krb5_read_password(context, promptstr, NULL, password.data,
                                    &password.length);
        if (retval)
            goto cleanup;

        retval = get_etype_info(context, princ, fetch, salt_str,
                                &enctype, &salt, &params);
        if (retval)
            goto cleanup;
        s2kparams = (params.length > 0) ? &params : NULL;
        retval = krb5_c_string_to_key_with_params(context, enctype, &password,
                                                  &salt, s2kparams, &key);
        if (retval)
            goto cleanup;
        entry->key = key;
    } else {
        printf(_("Key for %s (hex): "), princ_full);
        fgets(buf, BUFSIZ, stdin);
        /*
         * We need to get rid of the trailing '\n' from fgets.
         * If we have an even number of hex digits (as we should),
         * write a '\0' over the '\n'.  If for some reason we have
         * an odd number of hex digits, force an even number of hex
         * digits by writing a '0' into the last position (the string
         * will still be null-terminated).
         */
        buf[strlen(buf) - 1] = strlen(buf) % 2 ? '\0' : '0';
        if (strlen(buf) == 0) {
            fprintf(stderr, _("addent: Error reading key.\n"));
            retval = 0;
            goto cleanup;
        }

        retval = k5_hex_decode(buf, &keybytes, &keylen);
        if (retval) {
            if (retval == EINVAL) {
                fprintf(stderr, _("addent: Illegal character in key.\n"));
                retval = 0;
            }
            goto cleanup;
        }

        entry->key.enctype = enctype;
        entry->key.contents = keybytes;
        entry->key.length = keylen;
    }
    entry->principal = princ;
    entry->vno = kvno;
    entry->timestamp = now;

    /* Add entry to the end of the list (or create a new list if empty). */
    lp = k5alloc(sizeof(*lp), &retval);
    if (lp == NULL)
        goto cleanup;
    lp->next = NULL;
    lp->entry = entry;
    entry = NULL;
    for (last = list; *last != NULL; last = &(*last)->next);
    *last = lp;

cleanup:
    krb5_free_keytab_entry_contents(context, entry);
    free(entry);
    zapfree(password.data, password.length);
    krb5_free_data_contents(context, &salt);
    krb5_free_data_contents(context, &params);
    krb5_free_unparsed_name(context, princ_full);
    return retval;
}

/*
 * Read in a keytab and append it to list.  If list starts as NULL,
 * allocate a new one if necessary.
 */
krb5_error_code ktutil_read_keytab(context, name, list)
    krb5_context context;
    char *name;
    krb5_kt_list *list;
{
    krb5_kt_list lp = NULL, tail = NULL, back = NULL;
    krb5_keytab kt;
    krb5_keytab_entry *entry;
    krb5_kt_cursor cursor;
    krb5_error_code retval = 0;

    if (*list) {
        /* point lp at the tail of the list */
        for (lp = *list; lp->next; lp = lp->next);
        back = lp;
    }
    retval = krb5_kt_resolve(context, name, &kt);
    if (retval)
        return retval;
    retval = krb5_kt_start_seq_get(context, kt, &cursor);
    if (retval)
        goto close_kt;
    for (;;) {
        entry = (krb5_keytab_entry *)malloc(sizeof (krb5_keytab_entry));
        if (!entry) {
            retval = ENOMEM;
            break;
        }
        memset(entry, 0, sizeof (*entry));
        retval = krb5_kt_next_entry(context, kt, entry, &cursor);
        if (retval)
            break;

        if (!lp) {              /* if list is empty, start one */
            lp = (krb5_kt_list)malloc(sizeof (*lp));
            if (!lp) {
                retval = ENOMEM;
                break;
            }
        } else {
            lp->next = (krb5_kt_list)malloc(sizeof (*lp));
            if (!lp->next) {
                retval = ENOMEM;
                break;
            }
            lp = lp->next;
        }
        if (!tail)
            tail = lp;
        lp->next = NULL;
        lp->entry = entry;
    }
    if (entry)
        free(entry);
    if (retval) {
        if (retval == KRB5_KT_END)
            retval = 0;
        else {
            ktutil_free_kt_list(context, tail);
            tail = NULL;
            if (back)
                back->next = NULL;
        }
    }
    if (!*list)
        *list = tail;
    krb5_kt_end_seq_get(context, kt, &cursor);
close_kt:
    krb5_kt_close(context, kt);
    return retval;
}

/*
 * Takes a kt_list and writes it to the named keytab.
 */
krb5_error_code ktutil_write_keytab(context, list, name)
    krb5_context context;
    krb5_kt_list list;
    char *name;
{
    krb5_kt_list lp;
    krb5_keytab kt;
    char ktname[MAXPATHLEN+sizeof("WRFILE:")+1];
    krb5_error_code retval = 0;
    int result;

    result = snprintf(ktname, sizeof(ktname), "WRFILE:%s", name);
    if (SNPRINTF_OVERFLOW(result, sizeof(ktname)))
        return ENAMETOOLONG;
    retval = krb5_kt_resolve(context, ktname, &kt);
    if (retval)
        return retval;
    for (lp = list; lp; lp = lp->next) {
        retval = krb5_kt_add_entry(context, kt, lp->entry);
        if (retval)
            break;
    }
    krb5_kt_close(context, kt);
    return retval;
}
