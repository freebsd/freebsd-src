/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/kadm5/srv/pwqual_dict.c */
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
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 */

/* Password quality module to look up passwords within the realm dictionary. */

#include "k5-platform.h"
#include <krb5/pwqual_plugin.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <kadm5/admin.h>
#include "adm_proto.h"
#include <syslog.h>
#include "server_internal.h"

typedef struct dict_moddata_st {
    char **word_list;        /* list of word pointers */
    char *word_block;        /* actual word data */
    unsigned int word_count; /* number of words */
} *dict_moddata;


/*
 * Function: word_compare
 *
 * Purpose: compare two words in the dictionary.
 *
 * Arguments:
 *      w1              (input) pointer to first word
 *      w2              (input) pointer to second word
 *      <return value>  result of strcmp
 *
 * Requires:
 *      w1 and w2 to point to valid memory
 *
 */

static int
word_compare(const void *s1, const void *s2)
{
    return (strcasecmp(*(const char **)s1, *(const char **)s2));
}

/*
 * Function: init-dict
 *
 * Purpose: Initialize in memory word dictionary
 *
 * Arguments:
 *          none
 *          <return value> KADM5_OK on success errno on failure;
 *                         (but success on ENOENT)
 *
 * Requires:
 *      If WORDFILE exists, it must contain a list of words,
 *      one word per-line.
 *
 * Effects:
 *      If WORDFILE exists, it is read into memory sorted for future
 * use.  If it does not exist, it syslogs an error message and returns
 * success.
 *
 * Modifies:
 *      word_list to point to a chunck of allocated memory containing
 *      pointers to words
 *      word_block to contain the dictionary.
 *
 */

static int
init_dict(dict_moddata dict, const char *dict_file)
{
    int fd;
    size_t len, i;
    char *p, *t;
    struct stat sb;

    if (dict_file == NULL) {
        krb5_klog_syslog(LOG_INFO,
                         _("No dictionary file specified, continuing without "
                           "one."));
        return KADM5_OK;
    }
    if ((fd = open(dict_file, O_RDONLY)) == -1) {
        if (errno == ENOENT) {
            krb5_klog_syslog(LOG_ERR,
                             _("WARNING!  Cannot find dictionary file %s, "
                               "continuing without one."), dict_file);
            return KADM5_OK;
        } else
            return errno;
    }
    set_cloexec_fd(fd);
    if (fstat(fd, &sb) == -1) {
        close(fd);
        return errno;
    }
    if ((dict->word_block = malloc(sb.st_size + 1)) == NULL)
        return ENOMEM;
    if (read(fd, dict->word_block, sb.st_size) != sb.st_size)
        return errno;
    (void) close(fd);
    dict->word_block[sb.st_size] = '\0';

    p = dict->word_block;
    len = sb.st_size;
    while(len > 0 && (t = memchr(p, '\n', len)) != NULL) {
        *t = '\0';
        len -= t - p + 1;
        p = t + 1;
        dict->word_count++;
    }
    if ((dict->word_list = malloc(dict->word_count * sizeof(char *))) == NULL)
        return ENOMEM;
    p = dict->word_block;
    for (i = 0; i < dict->word_count; i++) {
        dict->word_list[i] = p;
        p += strlen(p) + 1;
    }
    qsort(dict->word_list, dict->word_count, sizeof(char *), word_compare);
    return KADM5_OK;
}

/*
 * Function: destroy_dict
 *
 * Purpose: destroy in-core copy of dictionary.
 *
 * Arguments:
 *          none
 *          <return value>  none
 * Requires:
 *          nothing
 * Effects:
 *      frees up memory occupied by word_list and word_block
 *      sets count back to 0, and resets the pointers to NULL
 *
 * Modifies:
 *      word_list, word_block, and word_count.
 *
 */

static void
destroy_dict(dict_moddata dict)
{
    if (dict == NULL)
        return;
    free(dict->word_list);
    free(dict->word_block);
    free(dict);
    return;
}

/* Implement the password quality open method by reading in dict_file. */
static krb5_error_code
dict_open(krb5_context context, const char *dict_file,
          krb5_pwqual_moddata *data)
{
    krb5_error_code ret;
    dict_moddata dict;

    *data = NULL;

    /* Allocate and initialize a dictionary structure. */
    dict = malloc(sizeof(*dict));
    if (dict == NULL)
        return ENOMEM;
    dict->word_list = NULL;
    dict->word_block = NULL;
    dict->word_count = 0;

    /* Fill in the dictionary structure with data from dict_file. */
    ret = init_dict(dict, dict_file);
    if (ret != 0) {
        destroy_dict(dict);
        return ret;
    }

    *data = (krb5_pwqual_moddata)dict;
    return 0;
}

/* Implement the password quality check method by checking the password
 * against the dictionary, as well as against principal components. */
static krb5_error_code
dict_check(krb5_context context, krb5_pwqual_moddata data,
           const char *password, const char *policy_name,
           krb5_principal princ, const char **languages)
{
    dict_moddata dict = (dict_moddata)data;

    /* Don't check the dictionary for principals with no password policy. */
    if (policy_name == NULL)
        return 0;

    /* Check against words in the dictionary if we successfully loaded one. */
    if (dict->word_list != NULL &&
        bsearch(&password, dict->word_list, dict->word_count, sizeof(char *),
                word_compare) != NULL)
        return KADM5_PASS_Q_DICT;

    return 0;
}

/* Implement the password quality close method. */
static void
dict_close(krb5_context context, krb5_pwqual_moddata data)
{
    destroy_dict((dict_moddata)data);
}

krb5_error_code
pwqual_dict_initvt(krb5_context context, int maj_ver, int min_ver,
                   krb5_plugin_vtable vtable)
{
    krb5_pwqual_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_pwqual_vtable)vtable;
    vt->name = "dict";
    vt->open = dict_open;
    vt->check = dict_check;
    vt->close = dict_close;
    return 0;
}
