/*-
 * Copyright (c) 1999 Michael Smith <msmith@freebsd.org>
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
 *
 *	$FreeBSD$
 */

#include <stand.h>
#include "libofw.h"

#if defined(LOADER_NET_SUPPORT)
#include "dev_net.h"
#endif

/*
 * We could use linker sets for some or all of these, but
 * then we would have to control what ended up linked into
 * the bootstrap.  So it's easier to conditionalise things
 * here.
 *
 * XXX rename these arrays to be consistent and less namespace-hostile
 */

/* Exported for libstand */
struct devsw *devsw[] = {
#if defined(LOADER_DISK_SUPPORT) || defined(LOADER_CDROM_SUPPORT)
    &ofwdisk,
#endif
#if defined(LOADER_NET_SUPPORT)
    &ofwnet,
  &ofwnet,
#endif
    NULL
};

struct fs_ops *file_system[] = {
#ifdef LOADER_UFS_SUPPORT
    &ufs_fsops,
#endif
#ifdef LOADER_CDROM_SUPPORT
    &cd9660_fsops,
#endif
#ifdef LOADER_EXT2FS_SUPPORT
    &ext2fs_fsops,
#endif
#ifdef LOADER_NET_SUPPORT
    &nfs_fsops,
#endif
    &zipfs_fsops,
    NULL
};

extern struct netif_driver of_net;

struct netif_driver *netif_drivers[] = {
#ifdef LOADER_NET_SUPPORT
    &of_net,
#endif
    NULL,
};

/* Exported for PowerPC only */
/* 
 * Sort formats so that those that can detect based on arguments
 * rather than reading the file go first.
 */
extern struct file_format powerpc_elf;

struct file_format *file_formats[] = {
/*  &powerpc_elf,*/
    NULL
};

/* 
 * Consoles 
 *
 * We don't prototype these in libalpha.h because they require
 * data structures from bootstrap.h as well.
 */
extern struct console ofwconsole;

struct console *consoles[] = {
    &ofwconsole,
    NULL
};
