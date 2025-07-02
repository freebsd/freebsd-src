/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/path.c - Portable path manipulation functions */
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

#include <k5-platform.h>

/* For testing purposes, use a different symbol for Windows path semantics. */
#ifdef _WIN32
#define WINDOWS_PATHS
#endif

/*
 * This file implements a limited set of portable path manipulation functions.
 * When in doubt about edge cases, we follow the Python os.path semantics.
 */

#ifdef WINDOWS_PATHS
#define SEP '\\'
#define IS_SEPARATOR(c) ((c) == '\\' || (c) == '/')
#else
#define SEP '/'
#define IS_SEPARATOR(c) ((c) == '/')
#endif

/* Find the rightmost path separator in path, or NULL if there is none. */
static inline const char *
find_sep(const char *path)
{
#ifdef WINDOWS_PATHS
    const char *slash, *backslash;

    slash = strrchr(path, '/');
    backslash = strrchr(path, '\\');
    if (slash != NULL && backslash != NULL)
        return (slash > backslash) ? slash : backslash;
    else
        return (slash != NULL) ? slash : backslash;
#else
    return strrchr(path, '/');
#endif
}

/* XXX drive letter prefixes */
long
k5_path_split(const char *path, char **parent_out, char **basename_out)
{
    const char *pathstart, *sep, *pend, *bstart;
    char *parent = NULL, *basename = NULL;

    if (parent_out != NULL)
        *parent_out = NULL;
    if (basename_out != NULL)
        *basename_out = NULL;

    pathstart = path;
#ifdef WINDOWS_PATHS
    if (*path != '\0' && path[1] == ':')
        pathstart = path + 2;
#endif

    sep = find_sep(pathstart);
    if (sep != NULL) {
        bstart = sep + 1;
        /* Strip off excess separators before the one we found. */
        pend = sep;
        while (pend > pathstart && IS_SEPARATOR(pend[-1]))
            pend--;
        /* But if we hit the start, keep the whole separator sequence. */
        if (pend == pathstart)
            pend = sep + 1;
    } else {
        bstart = pathstart;
        pend = pathstart;
    }

    if (parent_out) {
        parent = malloc(pend - path + 1);
        if (parent == NULL)
            return ENOMEM;
        memcpy(parent, path, pend - path);
        parent[pend - path] = '\0';
    }
    if (basename_out) {
        basename = strdup(bstart);
        if (basename == NULL) {
            free(parent);
            return ENOMEM;
        }
    }

    if (parent_out)
        *parent_out = parent;
    if (basename_out)
        *basename_out = basename;
    return 0;
}

long
k5_path_join(const char *path1, const char *path2, char **path_out)
{
    char *path, c;
    int ret;

    *path_out = NULL;
    if (k5_path_isabs(path2) || *path1 == '\0') {
        /* Discard path1 and return a copy of path2. */
        path = strdup(path2);
        if (path == NULL)
            return ENOMEM;
    } else {
        /*
         * Compose path1 and path2, adding a separator if path1 is non-empty
         * there's no separator between them already.  (*path2 can be a
         * separator in the weird case where it starts with /: or \: on
         * Windows, and Python doesn't insert a separator in this case.)
         */
        c = path1[strlen(path1) - 1];
        if (IS_SEPARATOR(c) || IS_SEPARATOR(*path2))
            ret = asprintf(&path, "%s%s", path1, path2);
        else
            ret = asprintf(&path, "%s%c%s", path1, SEP, path2);
        if (ret < 0)
            return ENOMEM;
    }
    *path_out = path;
    return 0;
}

int
k5_path_isabs(const char *path)
{
#ifdef WINDOWS_PATHS
    if (*path != '\0' && path[1] == ':')
        path += 2;
    return (*path == '/' || *path == '\\');
#else
    return (*path == '/');
#endif
}
