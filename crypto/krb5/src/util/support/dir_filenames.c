/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/dir_filenames.c - fetch filenames in a directory */
/*
 * Copyright (C) 2018 by the Massachusetts Institute of Technology.
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

#include "k5-platform.h"

void
k5_free_filenames(char **fnames)
{
    char **fn;

    for (fn = fnames; fn != NULL && *fn != NULL; fn++)
        free(*fn);
    free(fnames);
}

/* Resize the filename list and add a name. */
static int
add_filename(char ***fnames, int *n_fnames, const char *name)
{
    char **newlist;

    newlist = realloc(*fnames, (*n_fnames + 2) * sizeof(*newlist));
    if (newlist == NULL)
        return ENOMEM;
    *fnames = newlist;
    newlist[*n_fnames] = strdup(name);
    if (newlist[*n_fnames] == NULL)
        return ENOMEM;
    (*n_fnames)++;
    newlist[*n_fnames] = NULL;
    return 0;
}

static int
compare_with_strcmp(const void *a, const void *b)
{
    return strcmp(*(char **)a, *(char **)b);
}

#ifdef _WIN32

int
k5_dir_filenames(const char *dirname, char ***fnames_out)
{
    char *wildcard;
    WIN32_FIND_DATA ffd;
    HANDLE handle;
    char **fnames = NULL;
    int n_fnames = 0;

    *fnames_out = NULL;

    if (asprintf(&wildcard, "%s\\*", dirname) < 0)
        return ENOMEM;
    handle = FindFirstFile(wildcard, &ffd);
    free(wildcard);
    if (handle == INVALID_HANDLE_VALUE)
        return ENOENT;

    do {
        if (add_filename(&fnames, &n_fnames, ffd.cFileName) != 0) {
            k5_free_filenames(fnames);
            FindClose(handle);
            return ENOMEM;
        }
    } while (FindNextFile(handle, &ffd) != 0);

    FindClose(handle);
    qsort(fnames, n_fnames, sizeof(*fnames), compare_with_strcmp);
    *fnames_out = fnames;
    return 0;
}

#else /* _WIN32 */

#include <dirent.h>

int
k5_dir_filenames(const char *dirname, char ***fnames_out)
{
    DIR *dir;
    struct dirent *ent;
    char **fnames = NULL;
    int n_fnames = 0;

    *fnames_out = NULL;

    dir = opendir(dirname);
    if (dir == NULL)
        return ENOENT;

    while ((ent = readdir(dir)) != NULL) {
        if (add_filename(&fnames, &n_fnames, ent->d_name) != 0) {
            k5_free_filenames(fnames);
            closedir(dir);
            return ENOMEM;
        }
    }

    closedir(dir);
    qsort(fnames, n_fnames, sizeof(*fnames), compare_with_strcmp);
    *fnames_out = fnames;
    return 0;
}

#endif /* not _WIN32 */
