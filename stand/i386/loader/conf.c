/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
#include <bootstrap.h>
#include "libi386/libi386.h"
#if defined(LOADER_ZFS_SUPPORT)
#include "libzfs.h"
#endif

/*
 * We could use linker sets for some or all of these, but
 * then we would have to control what ended up linked into
 * the bootstrap.  So it's easier to conditionalise things
 * here.
 *
 * XXX rename these arrays to be consistent and less namespace-hostile
 *
 * XXX as libi386 and biosboot merge, some of these can become linker sets.
 */

extern struct devsw vdisk_dev;

/* Exported for libsa */
struct devsw *devsw[] = {
    &biosfd,
    &bioscd,
    &bioshd,
#if defined(LOADER_NFS_SUPPORT) || defined(LOADER_TFTP_SUPPORT)
    &pxedisk,
#endif
    &vdisk_dev,
#if defined(LOADER_ZFS_SUPPORT)
    &zfs_dev,
#endif
    NULL
};

struct fs_ops *file_system[] = {
#if defined(LOADER_ZFS_SUPPORT)
    &zfs_fsops,
#endif
#if defined(LOADER_UFS_SUPPORT)
    &ufs_fsops,
#endif
#if defined(LOADER_EXT2FS_SUPPORT)
    &ext2fs_fsops,
#endif
#if defined(LOADER_MSDOS_SUPPORT)
    &dosfs_fsops,
#endif
#if defined(LOADER_CD9660_SUPPORT)
    &cd9660_fsops,
#endif
#ifdef LOADER_NFS_SUPPORT 
    &nfs_fsops,
#endif
#ifdef LOADER_TFTP_SUPPORT
    &tftp_fsops,
#endif
#ifdef LOADER_GZIP_SUPPORT
    &gzipfs_fsops,
#endif
#ifdef LOADER_BZIP2_SUPPORT
    &bzipfs_fsops,
#endif
#ifdef LOADER_SPLIT_SUPPORT
    &splitfs_fsops,
#endif
    NULL
};

/* Exported for i386 only */
/* 
 * Sort formats so that those that can detect based on arguments
 * rather than reading the file go first.
 */
extern struct file_format	i386_elf;
extern struct file_format	i386_elf_obj;
extern struct file_format	amd64_elf;
extern struct file_format	amd64_elf_obj;
extern struct file_format	multiboot;
extern struct file_format	multiboot_obj;

struct file_format *file_formats[] = {
	&multiboot,
	&multiboot_obj,
#ifdef LOADER_PREFER_AMD64
    &amd64_elf,
    &amd64_elf_obj,
#endif
    &i386_elf,
    &i386_elf_obj,
#ifndef LOADER_PREFER_AMD64
    &amd64_elf,
    &amd64_elf_obj,
#endif
    NULL
};

/* 
 * Consoles 
 *
 * We don't prototype these in libi386.h because they require
 * data structures from bootstrap.h as well.
 */
extern struct console textvidc;
extern struct console vidconsole;
extern struct console comconsole;
extern struct console nullconsole;
extern struct console spinconsole;

struct console *consoles[] = {
#ifdef BIOS_TEXT_ONLY	/* Note: We need a forced commit for this */
    &textvidc,
#else
    &vidconsole,
#endif
    &comconsole,
    &nullconsole,
    &spinconsole,
    NULL
};

extern struct pnphandler isapnphandler;
extern struct pnphandler biospnphandler;
extern struct pnphandler biospcihandler;

struct pnphandler *pnphandlers[] = {
    &biospnphandler,		/* should go first, as it may set isapnp_readport */
    &isapnphandler,
    &biospcihandler,
    NULL
};
