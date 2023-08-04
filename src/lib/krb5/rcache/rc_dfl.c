/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/rcache/rc_dfl.c - default replay cache type */
/*
 * Copyright (C) 2019 by the Massachusetts Institute of Technology.
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
 * The dfl rcache type is a wrapper around the file2 rcache type, selecting a
 * filename and (on Unix-like systems) applying open() safety appropriate for
 * using a shared temporary directory.
 */

#include "k5-int.h"
#include "rc-int.h"
#ifdef _WIN32
#include "../os/os-proto.h"
#else
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifdef _WIN32

static krb5_error_code
open_file(krb5_context context, int *fd_out)
{
    krb5_error_code ret;
    char *fname;
    const char *dir;

    *fd_out = -1;

    dir = getenv("KRB5RCACHEDIR");
    if (dir != NULL) {
        if (asprintf(&fname, "%s\\krb5.rcache2") < 0)
            return ENOMEM;
    } else {
        ret = k5_expand_path_tokens(context, "%{LOCAL_APPDATA}\\krb5.rcache2",
                                    &fname);
        if (ret)
            return ret;
    }

    *fd_out = open(fname, O_CREAT | O_RDWR | O_BINARY, 0600);
    ret = (*fd_out < 0) ? errno : 0;
    if (ret) {
        k5_setmsg(context, ret, "%s (filename: %s)",
                  error_message(ret), fname);
    }
    free(fname);
    return ret;
}

#else /* _WIN32 */

static krb5_error_code
open_file(krb5_context context, int *fd_out)
{
    krb5_error_code ret;
    int fd = -1;
    char *fname = NULL;
    const char *dir;
    struct stat statbuf;
    uid_t euid = geteuid();

    *fd_out = -1;

    dir = secure_getenv("KRB5RCACHEDIR");
    if (dir == NULL) {
        dir = secure_getenv("TMPDIR");
        if (dir == NULL)
            dir = RCTMPDIR;
    }
    if (asprintf(&fname, "%s/krb5_%lu.rcache2", dir, (unsigned long)euid) < 0)
        return ENOMEM;

    fd = open(fname, O_CREAT | O_RDWR | O_NOFOLLOW, 0600);
    if (fd < 0) {
        ret = errno;
        k5_setmsg(context, ret, "%s (filename: %s)",
                  error_message(ret), fname);
        goto cleanup;
    }

    if (fstat(fd, &statbuf) < 0 || statbuf.st_uid != euid) {
        ret = EIO;
        k5_setmsg(context, ret, "Replay cache file %s is not owned by uid %lu",
                  fname, (unsigned long)euid);
        goto cleanup;
    }

    *fd_out = fd;
    fd = -1;
    ret = 0;

cleanup:
    if (fd != -1)
        close(fd);
    free(fname);
    return ret;
}

#endif /* not _WIN32 */

static krb5_error_code
dfl_resolve(krb5_context context, const char *residual, void **rcdata_out)
{
    *rcdata_out = NULL;
    return 0;
}

static void
dfl_close(krb5_context context, void *rcdata)
{
}

static krb5_error_code
dfl_store(krb5_context context, void *rcdata, const krb5_data *tag)
{
    krb5_error_code ret;
    int fd;

    ret = open_file(context, &fd);
    if (ret)
        return ret;

    ret = k5_rcfile2_store(context, fd, tag);
    close(fd);
    return ret;
}

const krb5_rc_ops k5_rc_dfl_ops =
{
    "dfl",
    dfl_resolve,
    dfl_close,
    dfl_store
};
