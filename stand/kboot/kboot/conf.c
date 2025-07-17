/*-
 * Copyright (C) 1999 Michael Smith <msmith@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stand.h>
#include "bootstrap.h"

#if defined(LOADER_NET_SUPPORT)
#include "dev_net.h"
#endif
#ifdef LOADER_ZFS_SUPPORT
#include "libzfs.h"
#endif

extern struct devsw hostdisk;
extern struct devsw host_dev;

/*
 * We could use linker sets for some or all of these, but
 * then we would have to control what ended up linked into
 * the bootstrap.  So it's easier to conditionalise things
 * here.
 *
 * XXX rename these arrays to be consistent and less namespace-hostile
 */

/* Exported for libsa */
struct devsw *devsw[] = {
#if defined(LOADER_DISK_SUPPORT) || defined(LOADER_CD9660_SUPPORT)
    &hostdisk,
#endif
#if defined(LOADER_NET_SUPPORT)
    &netdev,
#endif
    &host_dev,
#if defined(LOADER_ZFS_SUPPORT)
    &zfs_dev,				/* Must be last */
#endif
    NULL
};

extern struct fs_ops hostfs_fsops;

struct fs_ops *file_system[] = {
#if defined(LOADER_UFS_SUPPORT)
    &ufs_fsops,
#endif
#if defined(LOADER_CD9660_SUPPORT)
    &cd9660_fsops,
#endif
#if defined(LOADER_EXT2FS_SUPPORT)
    &ext2fs_fsops,
#endif
#if defined(LOADER_NFS_SUPPORT)
    &nfs_fsops,
#endif
#if defined(LOADER_TFTP_SUPPORT)
    &tftp_fsops,
#endif
#if defined(LOADER_GZIP_SUPPORT)
    &gzipfs_fsops,
#endif
#if defined(LOADER_BZIP2_SUPPORT)
    &bzipfs_fsops,
#endif
#if defined(LOADER_MSDOS_SUPPORT)
    &dosfs_fsops,
#endif
#if defined(LOADER_ZFS_SUPPORT)
	&zfs_fsops,
#endif
    &hostfs_fsops,
    NULL
};

extern struct netif_driver kbootnet;

struct netif_driver *netif_drivers[] = {
#if 0 /* XXX */
#if defined(LOADER_NET_SUPPORT)
	&kbootnet,
#endif
#endif
	NULL,
};

/*
 * Consoles
 */
extern struct console hostconsole;

struct console *consoles[] = {
    &hostconsole,
    NULL
};
