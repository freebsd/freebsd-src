/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/krbfileio.c */
/*
 * Copyright (c) Hewlett-Packard Company 1991
 * Released to the Massachusetts Institute of Technology for inclusion
 * in the Kerberos source code distribution.
 *
 * Copyright 1991 by the Massachusetts Institute of Technology.
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

#ifdef MODULE_VERSION_ID
static char *VersionID = "@(#)krbfileio.c       2 - 08/22/91";
#endif


#include "k5-int.h"
#include "os-proto.h"
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#include <fcntl.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifdef apollo
#   define OPEN_MODE_NOT_TRUSTWORTHY
#endif

krb5_error_code
k5_create_secure_file(krb5_context context, const char *pathname)
{
    int fd;

    /*
     * Create the file with access restricted to the owner
     */
    fd = THREEPARAMOPEN(pathname, O_RDWR | O_CREAT | O_EXCL | O_BINARY, 0600);

#ifdef OPEN_MODE_NOT_TRUSTWORTHY
    /*
     * Some systems that support default acl inheritance do not
     * apply ownership information from the process - force the file
     * to have the proper info.
     */
    if (fd > -1) {
        uid_t   uid;
        gid_t   gid;

        uid = getuid();
        gid = getgid();

        fchown(fd, uid, gid);

        fchmod(fd, 0600);
    }
#endif /* OPEN_MODE_NOT_TRUSTWORTHY */

    if (fd > -1) {
        close(fd);
        return 0;
    } else {
        return errno;
    }
}

krb5_error_code
k5_sync_disk_file(krb5_context context, FILE *fp)
{
    fflush(fp);
#if !defined(MSDOS_FILESYSTEM)
    if (fsync(fileno(fp))) {
        return errno;
    }
#endif

    return 0;
}
