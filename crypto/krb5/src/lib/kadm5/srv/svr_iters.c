/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 * $Header$
 */

#include "autoconf.h"
#if defined(HAVE_COMPILE) && defined(HAVE_STEP)
#define SOLARIS_REGEXPS
#elif defined(HAVE_REGCOMP) && defined(HAVE_REGEXEC)
#define POSIX_REGEXPS
#elif defined(HAVE_RE_COMP) && defined(HAVE_RE_EXEC)
#define BSD_REGEXPS
#else
#error I cannot find any regexp functions
#endif

#include        <sys/types.h>
#include        <string.h>
#include        <kadm5/admin.h>
#ifdef SOLARIS_REGEXPS
#include        <regexpr.h>
#endif
#ifdef POSIX_REGEXPS
#include        <regex.h>
#endif
#include <stdlib.h>

#include        "server_internal.h"

struct iter_data {
    krb5_context context;
    char **names;
    int n_names, sz_names;
    unsigned int malloc_failed;
    char *exp;
#ifdef SOLARIS_REGEXPS
    char *expbuf;
#endif
#ifdef POSIX_REGEXPS
    regex_t preg;
#endif
};

/* XXX Duplicated in kdb5_util!  */
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
static kadm5_ret_t glob_to_regexp(char *glob, char *realm, char **regexp)
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
    return KADM5_OK;
}

static void get_either_iter(struct iter_data *data, char *name)
{
    int match;
#ifdef SOLARIS_REGEXPS
    match = (step(name, data->expbuf) != 0);
#endif
#ifdef POSIX_REGEXPS
    match = (regexec(&data->preg, name, 0, NULL, 0) == 0);
#endif
#ifdef BSD_REGEXPS
    match = (re_exec(name) != 0);
#endif
    if (match) {
        if (data->n_names == data->sz_names) {
            int new_sz = data->sz_names * 2;
            char **new_names = realloc(data->names,
                                       new_sz * sizeof(char *));
            if (new_names) {
                data->names = new_names;
                data->sz_names = new_sz;
            } else {
                data->malloc_failed = 1;
                free(name);
                return;
            }
        }
        data->names[data->n_names++] = name;
    } else
        free(name);
}

static void get_pols_iter(void *data, osa_policy_ent_t entry)
{
    char *name;

    if ((name = strdup(entry->name)) == NULL)
        return;
    get_either_iter(data, name);
}

static void get_princs_iter(void *data, krb5_principal princ)
{
    struct iter_data *id = (struct iter_data *) data;
    char *name;

    if (krb5_unparse_name(id->context, princ, &name) != 0)
        return;
    get_either_iter(data, name);
}

static kadm5_ret_t kadm5_get_either(int princ,
                                    void *server_handle,
                                    char *exp,
                                    char ***princs,
                                    int *count)
{
    struct iter_data data;
#ifdef BSD_REGEXPS
    char *msg;
#endif
    char *regexp = NULL;
    int i, ret;
    kadm5_server_handle_t handle = server_handle;

    *princs = NULL;
    *count = 0;
    if (exp == NULL)
        exp = "*";

    CHECK_HANDLE(server_handle);

    if ((ret = glob_to_regexp(exp, princ ? handle->params.realm : NULL,
                              &regexp)) != KADM5_OK)
        return ret;

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
    )
    {
        /* XXX syslog msg or regerr(regerrno) */
        free(regexp);
        return EINVAL;
    }

    data.n_names = 0;
    data.sz_names = 10;
    data.malloc_failed = 0;
    data.names = malloc(sizeof(char *) * data.sz_names);
    if (data.names == NULL) {
        free(regexp);
        return ENOMEM;
    }

    if (princ) {
        data.context = handle->context;
        ret = kdb_iter_entry(handle, exp, get_princs_iter, (void *) &data);
    } else {
        ret = krb5_db_iter_policy(handle->context, exp, get_pols_iter, (void *)&data);
    }

    free(regexp);
#ifdef POSIX_REGEXPS
    regfree(&data.preg);
#endif
    if ( !ret && data.malloc_failed)
        ret = ENOMEM;
    if ( ret ) {
        for (i = 0; i < data.n_names; i++)
            free(data.names[i]);
        free(data.names);
        return ret;
    }

    *princs = data.names;
    *count = data.n_names;
    return KADM5_OK;
}

kadm5_ret_t kadm5_get_principals(void *server_handle,
                                 char *exp,
                                 char ***princs,
                                 int *count)
{
    return kadm5_get_either(1, server_handle, exp, princs, count);
}

kadm5_ret_t kadm5_get_policies(void *server_handle,
                               char *exp,
                               char ***pols,
                               int *count)
{
    return kadm5_get_either(0, server_handle, exp, pols, count);
}
