/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/pwqual/test/main.c - test module for password quality interface */
/*
 * Copyright (C) 2010,2013 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file implements a module named "combo" which tests whether a password
 * matches a pair of words in the dictionary.  It also implements several dummy
 * modules named "dyn1", "dyn2", and "dyn3" which are used for ordering tests.
 */

#include <k5-platform.h>
#include <krb5/pwqual_plugin.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct combo_moddata_st {
    const char **word_list;     /* list of word pointers */
    char *word_block;           /* actual word data */
} *combo_moddata;

static krb5_error_code
init_dict(combo_moddata dict, const char *dict_file)
{
    int fd;
    size_t count, len, i;
    char *p, *t;
    struct stat sb;

    /* Read the dictionary file into memory in one blob. */
    if (dict_file == NULL)
        return 0;
    fd = open(dict_file, O_RDONLY);
    if (fd == -1)
        return (errno == ENOENT) ? 0 : errno;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        return errno;
    }
    dict->word_block = malloc(sb.st_size + 1);
    if (dict->word_block == NULL)
        return ENOMEM;
    if (read(fd, dict->word_block, sb.st_size) != sb.st_size)
        return errno;
    close(fd);
    dict->word_block[sb.st_size] = '\0';

    /* Decompose the blob into newline-separated words. */
    p = dict->word_block;
    len = sb.st_size;
    count = 0;
    while (len > 0 && (t = memchr(p, '\n', len)) != NULL) {
        *t = '\0';
        len -= t - p + 1;
        p = t + 1;
        count++;
    }
    dict->word_list = calloc(count + 1, sizeof(char *));
    if (dict->word_list == NULL)
        return ENOMEM;
    p = dict->word_block;
    for (i = 0; i < count; i++) {
        dict->word_list[i] = p;
        p += strlen(p) + 1;
    }
    return 0;
}

static void
destroy_dict(combo_moddata dict)
{
    if (dict == NULL)
        return;
    free(dict->word_list);
    free(dict->word_block);
    free(dict);
}

static krb5_error_code
combo_open(krb5_context context, const char *dict_file,
           krb5_pwqual_moddata *data)
{
    krb5_error_code ret;
    combo_moddata dict;

    *data = NULL;

    /* Allocate and initialize a dictionary structure. */
    dict = malloc(sizeof(*dict));
    if (dict == NULL)
        return ENOMEM;
    dict->word_list = NULL;
    dict->word_block = NULL;

    /* Fill in the dictionary structure with data from dict_file. */
    ret = init_dict(dict, dict_file);
    if (ret != 0) {
        destroy_dict(dict);
        return ret;
    }

    *data = (krb5_pwqual_moddata)dict;
    return 0;
}

static krb5_error_code
combo_check(krb5_context context, krb5_pwqual_moddata data,
            const char *password, const char *policy_name,
            krb5_principal princ, const char **languages)
{
    combo_moddata dict = (combo_moddata)data;
    const char *remainder, **word1, **word2;

    if (dict->word_list == NULL)
        return 0;

    for (word1 = dict->word_list; *word1 != NULL; word1++) {
        if (strncasecmp(password, *word1, strlen(*word1)) != 0)
            continue;
        remainder = password + strlen(*word1);
        for (word2 = dict->word_list; *word2 != NULL; word2++) {
            if (strcasecmp(remainder, *word2) == 0) {
                krb5_set_error_message(context, KADM5_PASS_Q_DICT,
                                       "Password may not be a pair of "
                                       "dictionary words");
                return KADM5_PASS_Q_DICT;
            }
        }
    }

    return 0;
}

static void
combo_close(krb5_context context, krb5_pwqual_moddata data)
{
    destroy_dict((combo_moddata)data);
}

krb5_error_code
pwqual_combo_initvt(krb5_context context, int maj_ver, int min_ver,
                    krb5_plugin_vtable vtable);
krb5_error_code
pwqual_dyn1_initvt(krb5_context context, int maj_ver, int min_ver,
                   krb5_plugin_vtable vtable);
krb5_error_code
pwqual_dyn2_initvt(krb5_context context, int maj_ver, int min_ver,
                   krb5_plugin_vtable vtable);
krb5_error_code
pwqual_dyn3_initvt(krb5_context context, int maj_ver, int min_ver,
                   krb5_plugin_vtable vtable);

krb5_error_code
pwqual_combo_initvt(krb5_context context, int maj_ver, int min_ver,
                    krb5_plugin_vtable vtable)
{
    krb5_pwqual_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_pwqual_vtable)vtable;
    vt->name = "combo";
    vt->open = combo_open;
    vt->check = combo_check;
    vt->close = combo_close;
    return 0;
}

krb5_error_code
pwqual_dyn1_initvt(krb5_context context, int maj_ver, int min_ver,
                   krb5_plugin_vtable vtable)
{
    ((krb5_pwqual_vtable)vtable)->name = "dyn1";
    return 0;
}

krb5_error_code
pwqual_dyn2_initvt(krb5_context context, int maj_ver, int min_ver,
                   krb5_plugin_vtable vtable)
{
    ((krb5_pwqual_vtable)vtable)->name = "dyn2";
    return 0;
}

krb5_error_code
pwqual_dyn3_initvt(krb5_context context, int maj_ver, int min_ver,
                   krb5_plugin_vtable vtable)
{
    ((krb5_pwqual_vtable)vtable)->name = "dyn3";
    return 0;
}
