/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/cc_dir.c - Directory-based credential cache collection */
/*
 * Copyright (C) 2011 by the Massachusetts Institute of Technology.
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
 * This credential cache type represents a set of file-based caches with a
 * switchable primary cache.  An alternate form of the type represents a
 * subsidiary file cache within the directory.
 *
 * A cache name of the form DIR:dirname identifies a directory containing the
 * cache set.  Resolving a name of this form results in dirname's primary
 * cache.  If a context's default cache is of this form, the global cache
 * collection will contain dirname's cache set, and new unique caches of type
 * DIR will be created within dirname.
 *
 * A cache name of the form DIR::filepath represents a single cache within the
 * directory.  Switching to a ccache of this type causes the directory's
 * primary cache to be set to the named cache.
 *
 * Within the directory, cache names begin with 'tkt'.  The file "primary"
 * contains a single line naming the primary cache.  The directory must already
 * exist when the DIR ccache is resolved, but the primary file will be created
 * automatically if it does not exist.
 */

#include "k5-int.h"
#include "cc-int.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

/* This is Unix-only for now.  To work on Windows, we will need opendir/readdir
 * replacements and possibly more flexible newline handling. */
#ifndef _WIN32

#include <dirent.h>

extern const krb5_cc_ops krb5_dcc_ops;
extern const krb5_cc_ops krb5_fcc_ops;

/* Fields are not modified after creation, so no lock is necessary. */
typedef struct dcc_data_st {
    char *residual;             /* dirname or :filename */
    krb5_ccache fcc;            /* File cache for actual cache ops */
} dcc_data;

static inline krb5_boolean
filename_is_cache(const char *filename)
{
    return (strncmp(filename, "tkt", 3) == 0);
}

/* Compose the pathname of the primary file within a cache directory. */
static inline krb5_error_code
primary_pathname(const char *dirname, char **path_out)
{
    return k5_path_join(dirname, "primary", path_out);
}

/* Compose a residual string for a subsidiary path with the specified directory
 * name and filename. */
static krb5_error_code
subsidiary_residual(const char *dirname, const char *filename, char **out)
{
    krb5_error_code ret;
    char *path, *residual;

    *out = NULL;
    ret = k5_path_join(dirname, filename, &path);
    if (ret)
        return ret;
    ret = asprintf(&residual, ":%s", path);
    free(path);
    if (ret < 0)
        return ENOMEM;
    *out = residual;
    return 0;
}

static inline krb5_error_code
split_path(krb5_context context, const char *path, char **dirname_out,
           char **filename_out)
{
    krb5_error_code ret;
    char *dirname, *filename;

    *dirname_out = NULL;
    *filename_out = NULL;
    ret = k5_path_split(path, &dirname, &filename);
    if (ret)
        return ret;

    if (*dirname == '\0') {
        ret = KRB5_CC_BADNAME;
        k5_setmsg(context, ret,
                  _("Subsidiary cache path %s has no parent directory"), path);
        goto error;
    }
    if (!filename_is_cache(filename)) {
        ret = KRB5_CC_BADNAME;
        k5_setmsg(context, ret,
                  _("Subsidiary cache path %s filename does not begin with "
                    "\"tkt\""), path);
        goto error;
    }

    *dirname_out = dirname;
    *filename_out = filename;
    return 0;

error:
    free(dirname);
    free(filename);
    return ret;
}

/* Read the primary file and compose the residual string for the primary
 * subsidiary cache file. */
static krb5_error_code
read_primary_file(krb5_context context, const char *primary_path,
                  const char *dirname, char **residual_out)
{
    FILE *fp;
    char buf[64], *ret;
    size_t len;

    *residual_out = NULL;

    /* Open the file and read its first line. */
    fp = fopen(primary_path, "r");
    if (fp == NULL)
        return ENOENT;
    ret = fgets(buf, sizeof(buf), fp);
    fclose(fp);
    if (ret == NULL)
        return KRB5_CC_IO;
    len = strlen(buf);

    /* Check if line is too long, doesn't look like a subsidiary cache
     * filename, or isn't a single-component filename. */
    if (buf[len - 1] != '\n' || !filename_is_cache(buf) ||
        strchr(buf, '/') || strchr(buf, '\\')) {
        k5_setmsg(context, KRB5_CC_FORMAT, _("%s contains invalid filename"),
                  primary_path);
        return KRB5_CC_FORMAT;
    }
    buf[len - 1] = '\0';

    return subsidiary_residual(dirname, buf, residual_out);
}

/* Create or update the primary file with a line containing contents. */
static krb5_error_code
write_primary_file(const char *primary_path, const char *contents)
{
    krb5_error_code ret = KRB5_CC_IO;
    char *newpath = NULL;
    FILE *fp = NULL;
    int fd = -1, status;

    if (asprintf(&newpath, "%s.XXXXXX", primary_path) < 0)
        return ENOMEM;
    fd = mkstemp(newpath);
    if (fd < 0)
        goto cleanup;
#ifdef HAVE_CHMOD
    chmod(newpath, S_IRUSR | S_IWUSR);
#endif
    fp = fdopen(fd, "w");
    if (fp == NULL)
        goto cleanup;
    fd = -1;
    if (fprintf(fp, "%s\n", contents) < 0)
        goto cleanup;
    status = fclose(fp);
    fp = NULL;
    if (status == EOF)
        goto cleanup;
    fp = NULL;
    if (rename(newpath, primary_path) != 0)
        goto cleanup;
    ret = 0;

cleanup:
    if (fd >= 0)
        close(fd);
    if (fp != NULL)
        fclose(fp);
    free(newpath);
    return ret;
}

/* Verify or create a cache directory path. */
static krb5_error_code
verify_dir(krb5_context context, const char *dirname)
{
    struct stat st;

    if (stat(dirname, &st) < 0) {
        if (errno == ENOENT && mkdir(dirname, S_IRWXU) == 0)
            return 0;
        k5_setmsg(context, KRB5_FCC_NOFILE,
                  _("Credential cache directory %s does not exist"),
                  dirname);
        return KRB5_FCC_NOFILE;
    }
    if (!S_ISDIR(st.st_mode)) {
        k5_setmsg(context, KRB5_CC_FORMAT,
                  _("Credential cache directory %s exists but is not a "
                    "directory"), dirname);
        return KRB5_CC_FORMAT;
    }
    return 0;
}

/*
 * If the default ccache name for context is a directory collection, set
 * *dirname_out to the directory name for that collection.  Otherwise set
 * *dirname_out to NULL.
 */
static krb5_error_code
get_context_default_dir(krb5_context context, char **dirname_out)
{
    const char *defname;
    char *dirname;

    *dirname_out = NULL;
    defname = krb5_cc_default_name(context);
    if (defname == NULL)
        return 0;
    if (strncmp(defname, "DIR:", 4) != 0 ||
        defname[4] == ':' || defname[4] == '\0')
        return 0;
    dirname = strdup(defname + 4);
    if (dirname == NULL)
        return ENOMEM;
    *dirname_out = dirname;
    return 0;
}

/*
 * If the default ccache name for context is a subsidiary file in a directory
 * collection, set *subsidiary_out to the residual value.  Otherwise set
 * *subsidiary_out to NULL.
 */
static krb5_error_code
get_context_subsidiary_file(krb5_context context, char **subsidiary_out)
{
    const char *defname;
    char *residual;

    *subsidiary_out = NULL;
    defname = krb5_cc_default_name(context);
    if (defname == NULL || strncmp(defname, "DIR::", 5) != 0)
        return 0;
    residual = strdup(defname + 4);
    if (residual == NULL)
        return ENOMEM;
    *subsidiary_out = residual;
    return 0;
}

static const char * KRB5_CALLCONV
dcc_get_name(krb5_context context, krb5_ccache cache)
{
    dcc_data *data = cache->data;

    return data->residual;
}

/* Construct a cache object given a residual string and file ccache.  Take
 * ownership of fcc on success. */
static krb5_error_code
make_cache(const char *residual, krb5_ccache fcc, krb5_ccache *cache_out)
{
    krb5_ccache cache = NULL;
    dcc_data *data = NULL;
    char *residual_copy = NULL;

    cache = malloc(sizeof(*cache));
    if (cache == NULL)
        goto oom;
    data = malloc(sizeof(*data));
    if (data == NULL)
        goto oom;
    residual_copy = strdup(residual);
    if (residual_copy == NULL)
        goto oom;

    data->residual = residual_copy;
    data->fcc = fcc;
    cache->ops = &krb5_dcc_ops;
    cache->data = data;
    cache->magic = KV5M_CCACHE;
    *cache_out = cache;
    return 0;

oom:
    free(cache);
    free(data);
    free(residual_copy);
    return ENOMEM;
}

static krb5_error_code KRB5_CALLCONV
dcc_resolve(krb5_context context, krb5_ccache *cache_out, const char *residual)
{
    krb5_error_code ret;
    krb5_ccache fcc;
    char *primary_path = NULL, *sresidual = NULL, *dirname, *filename;

    *cache_out = NULL;

    if (*residual == ':') {
        /* This is a subsidiary cache within the directory. */
        ret = split_path(context, residual + 1, &dirname, &filename);
        if (ret)
            return ret;

        ret = verify_dir(context, dirname);
        free(dirname);
        free(filename);
        if (ret)
            return ret;
    } else {
        /* This is the directory itself; resolve to the primary cache. */
        ret = verify_dir(context, residual);
        if (ret)
            return ret;

        ret = primary_pathname(residual, &primary_path);
        if (ret)
            goto cleanup;

        ret = read_primary_file(context, primary_path, residual, &sresidual);
        if (ret == ENOENT) {
            /* Create an initial primary file. */
            ret = write_primary_file(primary_path, "tkt");
            if (ret)
                goto cleanup;
            ret = subsidiary_residual(residual, "tkt", &sresidual);
        }
        if (ret)
            goto cleanup;
        residual = sresidual;
    }

    ret = krb5_fcc_ops.resolve(context, &fcc, residual + 1);
    if (ret)
        goto cleanup;
    ret = make_cache(residual, fcc, cache_out);
    if (ret)
        krb5_fcc_ops.close(context, fcc);

cleanup:
    free(primary_path);
    free(sresidual);
    return ret;
}

static krb5_error_code KRB5_CALLCONV
dcc_gen_new(krb5_context context, krb5_ccache *cache_out)
{
    krb5_error_code ret;
    char *dirname = NULL, *template = NULL, *residual = NULL;
    krb5_ccache fcc = NULL;

    *cache_out = NULL;
    ret = get_context_default_dir(context, &dirname);
    if (ret)
        return ret;
    if (dirname == NULL) {
        k5_setmsg(context, KRB5_DCC_CANNOT_CREATE,
                  _("Can't create new subsidiary cache because default cache "
                    "is not a directory collection"));
        return KRB5_DCC_CANNOT_CREATE;
    }
    ret = verify_dir(context, dirname);
    if (ret)
        goto cleanup;
    ret = k5_path_join(dirname, "tktXXXXXX", &template);
    if (ret)
        goto cleanup;
    ret = krb5int_fcc_new_unique(context, template, &fcc);
    if (ret)
        goto cleanup;
    if (asprintf(&residual, ":%s", template) < 0) {
        ret = ENOMEM;
        goto cleanup;
    }
    ret = make_cache(residual, fcc, cache_out);
    if (ret)
        goto cleanup;
    fcc = NULL;

cleanup:
    if (fcc != NULL)
        krb5_fcc_ops.destroy(context, fcc);
    free(dirname);
    free(template);
    free(residual);
    return ret;
}

static krb5_error_code KRB5_CALLCONV
dcc_init(krb5_context context, krb5_ccache cache, krb5_principal princ)
{
    dcc_data *data = cache->data;

    return krb5_fcc_ops.init(context, data->fcc, princ);
}

static krb5_error_code KRB5_CALLCONV
dcc_destroy(krb5_context context, krb5_ccache cache)
{
    dcc_data *data = cache->data;
    krb5_error_code ret;

    ret = krb5_fcc_ops.destroy(context, data->fcc);
    free(data->residual);
    free(data);
    free(cache);
    return ret;
}

static krb5_error_code KRB5_CALLCONV
dcc_close(krb5_context context, krb5_ccache cache)
{
    dcc_data *data = cache->data;
    krb5_error_code ret;

    ret = krb5_fcc_ops.close(context, data->fcc);
    free(data->residual);
    free(data);
    free(cache);
    return ret;
}

static krb5_error_code KRB5_CALLCONV
dcc_store(krb5_context context, krb5_ccache cache, krb5_creds *creds)
{
    dcc_data *data = cache->data;

    return krb5_fcc_ops.store(context, data->fcc, creds);
}

static krb5_error_code KRB5_CALLCONV
dcc_retrieve(krb5_context context, krb5_ccache cache, krb5_flags flags,
             krb5_creds *mcreds, krb5_creds *creds)
{
    dcc_data *data = cache->data;

    return krb5_fcc_ops.retrieve(context, data->fcc, flags, mcreds,
                                 creds);
}

static krb5_error_code KRB5_CALLCONV
dcc_get_princ(krb5_context context, krb5_ccache cache,
              krb5_principal *princ_out)
{
    dcc_data *data = cache->data;

    return krb5_fcc_ops.get_princ(context, data->fcc, princ_out);
}

static krb5_error_code KRB5_CALLCONV
dcc_get_first(krb5_context context, krb5_ccache cache, krb5_cc_cursor *cursor)
{
    dcc_data *data = cache->data;

    return krb5_fcc_ops.get_first(context, data->fcc, cursor);
}

static krb5_error_code KRB5_CALLCONV
dcc_get_next(krb5_context context, krb5_ccache cache, krb5_cc_cursor *cursor,
             krb5_creds *creds)
{
    dcc_data *data = cache->data;

    return krb5_fcc_ops.get_next(context, data->fcc, cursor, creds);
}

static krb5_error_code KRB5_CALLCONV
dcc_end_get(krb5_context context, krb5_ccache cache, krb5_cc_cursor *cursor)
{
    dcc_data *data = cache->data;

    return krb5_fcc_ops.end_get(context, data->fcc, cursor);
}

static krb5_error_code KRB5_CALLCONV
dcc_remove_cred(krb5_context context, krb5_ccache cache, krb5_flags flags,
                krb5_creds *creds)
{
    dcc_data *data = cache->data;

    return krb5_fcc_ops.remove_cred(context, data->fcc, flags, creds);
}

static krb5_error_code KRB5_CALLCONV
dcc_set_flags(krb5_context context, krb5_ccache cache, krb5_flags flags)
{
    dcc_data *data = cache->data;

    return krb5_fcc_ops.set_flags(context, data->fcc, flags);
}

static krb5_error_code KRB5_CALLCONV
dcc_get_flags(krb5_context context, krb5_ccache cache, krb5_flags *flags_out)
{
    dcc_data *data = cache->data;

    return krb5_fcc_ops.get_flags(context, data->fcc, flags_out);
}

struct dcc_ptcursor_data {
    char *primary;
    char *dirname;
    DIR *dir;
    krb5_boolean first;
};

/* Construct a cursor, taking ownership of dirname, primary, and dir on
 * success. */
static krb5_error_code
make_cursor(char *dirname, char *primary, DIR *dir,
            krb5_cc_ptcursor *cursor_out)
{
    krb5_cc_ptcursor cursor;
    struct dcc_ptcursor_data *data;

    *cursor_out = NULL;

    data = malloc(sizeof(*data));
    if (data == NULL)
        return ENOMEM;
    cursor = malloc(sizeof(*cursor));
    if (cursor == NULL) {
        free(data);
        return ENOMEM;
    }

    data->dirname = dirname;
    data->primary = primary;
    data->dir = dir;
    data->first = TRUE;
    cursor->ops = &krb5_dcc_ops;
    cursor->data = data;
    *cursor_out = cursor;
    return 0;
}

static krb5_error_code KRB5_CALLCONV
dcc_ptcursor_new(krb5_context context, krb5_cc_ptcursor *cursor_out)
{
    krb5_error_code ret;
    char *dirname = NULL, *primary_path = NULL, *primary = NULL;
    DIR *dir = NULL;

    *cursor_out = NULL;

    /* If the default cache is a subsidiary file, make a cursor with the
     * specified file as the primary but with no directory collection. */
    ret = get_context_subsidiary_file(context, &primary);
    if (ret)
        goto cleanup;
    if (primary != NULL) {
        ret = make_cursor(NULL, primary, NULL, cursor_out);
        if (ret)
            free(primary);
        return ret;
    }

    /* Open the directory for the context's default cache. */
    ret = get_context_default_dir(context, &dirname);
    if (ret || dirname == NULL)
        goto cleanup;
    dir = opendir(dirname);
    if (dir == NULL)
        goto cleanup;

    /* Fetch the primary cache name if possible. */
    ret = primary_pathname(dirname, &primary_path);
    if (ret)
        goto cleanup;
    ret = read_primary_file(context, primary_path, dirname, &primary);
    if (ret)
        krb5_clear_error_message(context);

    ret = make_cursor(dirname, primary, dir, cursor_out);
    if (ret)
        goto cleanup;
    dirname = primary = NULL;
    dir = NULL;

cleanup:
    free(dirname);
    free(primary_path);
    free(primary);
    if (dir)
        closedir(dir);
    /* Return an empty cursor if we fail for any reason. */
    if (*cursor_out == NULL)
        return make_cursor(NULL, NULL, NULL, cursor_out);
    return 0;
}

static krb5_error_code KRB5_CALLCONV
dcc_ptcursor_next(krb5_context context, krb5_cc_ptcursor cursor,
                  krb5_ccache *cache_out)
{
    struct dcc_ptcursor_data *data = cursor->data;
    struct dirent *ent;
    char *residual;
    krb5_error_code ret;
    struct stat sb;

    *cache_out = NULL;

    /* Return the primary or specified subsidiary cache if we haven't yet. */
    if (data->first) {
        data->first = FALSE;
        if (data->primary != NULL && stat(data->primary + 1, &sb) == 0)
            return dcc_resolve(context, cache_out, data->primary);
    }

    if (data->dir == NULL)      /* No directory collection */
        return 0;

    /* Look for the next filename of the correct form, without repeating the
     * primary cache. */
    while ((ent = readdir(data->dir)) != NULL) {
        if (!filename_is_cache(ent->d_name))
            continue;
        ret = subsidiary_residual(data->dirname, ent->d_name, &residual);
        if (ret)
            return ret;
        if (data->primary != NULL && strcmp(residual, data->primary) == 0) {
            free(residual);
            continue;
        }
        ret = dcc_resolve(context, cache_out, residual);
        free(residual);
        return ret;
    }

    /* We exhausted the directory without finding a cache to yield. */
    closedir(data->dir);
    data->dir = NULL;
    return 0;
}

static krb5_error_code KRB5_CALLCONV
dcc_ptcursor_free(krb5_context context, krb5_cc_ptcursor *cursor)
{
    struct dcc_ptcursor_data *data = (*cursor)->data;

    if (data->dir)
        closedir(data->dir);
    free(data->dirname);
    free(data->primary);
    free(data);
    free(*cursor);
    *cursor = NULL;
    return 0;
}

static krb5_error_code KRB5_CALLCONV
dcc_lastchange(krb5_context context, krb5_ccache cache,
               krb5_timestamp *time_out)
{
    dcc_data *data = cache->data;

    return krb5_fcc_ops.lastchange(context, data->fcc, time_out);
}

static krb5_error_code KRB5_CALLCONV
dcc_lock(krb5_context context, krb5_ccache cache)
{
    dcc_data *data = cache->data;

    return krb5_fcc_ops.lock(context, data->fcc);
}

static krb5_error_code KRB5_CALLCONV
dcc_unlock(krb5_context context, krb5_ccache cache)
{
    dcc_data *data = cache->data;

    return krb5_fcc_ops.unlock(context, data->fcc);
}

static krb5_error_code KRB5_CALLCONV
dcc_switch_to(krb5_context context, krb5_ccache cache)
{
    dcc_data *data = cache->data;
    char *primary_path = NULL, *dirname = NULL, *filename = NULL;
    krb5_error_code ret;

    ret = split_path(context, data->residual + 1, &dirname, &filename);
    if (ret)
        return ret;

    ret = primary_pathname(dirname, &primary_path);
    if (ret)
        goto cleanup;

    ret = write_primary_file(primary_path, filename);

cleanup:
    free(primary_path);
    free(dirname);
    free(filename);
    return ret;
}

const krb5_cc_ops krb5_dcc_ops = {
    0,
    "DIR",
    dcc_get_name,
    dcc_resolve,
    dcc_gen_new,
    dcc_init,
    dcc_destroy,
    dcc_close,
    dcc_store,
    dcc_retrieve,
    dcc_get_princ,
    dcc_get_first,
    dcc_get_next,
    dcc_end_get,
    dcc_remove_cred,
    dcc_set_flags,
    dcc_get_flags,
    dcc_ptcursor_new,
    dcc_ptcursor_next,
    dcc_ptcursor_free,
    NULL, /* move */
    dcc_lastchange,
    NULL, /* wasdefault */
    dcc_lock,
    dcc_unlock,
    dcc_switch_to,
};

#endif /* not _WIN32 */
