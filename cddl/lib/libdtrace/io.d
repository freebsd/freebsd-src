/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 *
 * $FreeBSD$
 */
/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#pragma D depends_on provider io

typedef struct devinfo {
        int dev_major;                  /* major number */
        int dev_minor;                  /* minor number */
        int dev_instance;               /* instance number */
        string dev_name;                /* name of device */
        string dev_statname;            /* name of device + instance/minor */
        string dev_pathname;            /* pathname of device */
} devinfo_t;

#pragma D binding "1.0" translator
translator devinfo_t < struct devstat *D > {
           dev_major = D->device_number;
           dev_minor = D->unit_number;
           dev_instance = 0;
           dev_name = stringof(D->device_name);
           dev_statname = stringof(D->device_name);
           dev_pathname = stringof(D->device_name);
};

typedef struct bufinfo {
        int b_flags;                    /* flags */
        long b_bcount;                /* number of bytes */
        caddr_t b_addr;                 /* buffer address */
        uint64_t b_blkno;               /* expanded block # on device */
        uint64_t b_lblkno;              /* block # on device */
        size_t b_resid;                 /* # of bytes not transferred */
        size_t b_bufsize;               /* size of allocated buffer */
/*        caddr_t b_iodone;              I/O completion routine */
        int b_error;                    /* expanded error field */
/*        dev_t b_edev;                  extended device */
} bufinfo_t;

#pragma D binding "1.0" translator
translator bufinfo_t < struct bio *B > {
           b_flags = B->bio_flags;
           b_bcount = B->bio_bcount;
           b_addr = B->bio_data;
           b_blkno = 0;
           b_lblkno = 0;
           b_resid = B->bio_resid;
           b_bufsize = 0; /* XXX gnn */
           b_error = B->bio_error;
};

/*
 * The following inline constants can be used to examine fi_oflags when using
 * the fds[] array or a translated fileinfo_t.  Note that the various open
 * flags behave as a bit-field *except* for O_RDONLY, O_WRONLY, and O_RDWR.
 * To test the open mode, you write code similar to that used with the fcntl(2)
 * F_GET[X]FL command, such as: if ((fi_oflags & O_ACCMODE) == O_WRONLY).
 */
inline int O_ACCMODE = 0x0003;
#pragma D binding "1.1" O_ACCMODE

inline int O_RDONLY = 0x0000;
#pragma D binding "1.1" O_RDONLY
inline int O_WRONLY = 0x0001;
#pragma D binding "1.1" O_WRONLY
inline int O_RDWR = 0x0002;
#pragma D binding "1.1" O_RDWR

inline int O_APPEND = 0x0008;
#pragma D binding "1.1" O_APPEND
inline int O_CREAT = 0x0200;
#pragma D binding "1.1" O_CREAT
inline int O_EXCL = 0x0800;
#pragma D binding "1.1" O_EXCL
inline int O_NOCTTY = 0x8000;
#pragma D binding "1.1" O_NOCTTY
inline int O_NONBLOCK = 0x0004;
#pragma D binding "1.1" O_NONBLOCK
inline int O_NDELAY = 0x0004;
#pragma D binding "1.1" O_NDELAY
inline int O_SYNC = 0x0080;
#pragma D binding "1.1" O_SYNC
inline int O_TRUNC = 0x0400;
#pragma D binding "1.1" O_TRUNC


