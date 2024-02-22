/*-
 * Copyright (c) 2006 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stand.h>
#include <bootstrap.h>
#include <efi.h>
#include <efilib.h>
#include <efizfs.h>

extern struct devsw vdisk_dev;

#ifdef MD_IMAGE_SIZE
extern struct devsw md_dev;
#endif

struct devsw *devsw[] = {
	&efipart_fddev,
	&efipart_cddev,
	&efipart_hddev,
	&efihttp_dev, /* ordering with efinet_dev matters */
#if defined(LOADER_NET_SUPPORT)
	&efinet_dev,
#endif
	&vdisk_dev,
#ifdef EFI_ZFS_BOOT
	&zfs_dev,
#endif
#ifdef MD_IMAGE_SIZE
	&md_dev,
#endif
	NULL
};

struct fs_ops *file_system[] = {
#ifdef EFI_ZFS_BOOT
	&zfs_fsops,
#endif
	&dosfs_fsops,
	&ufs_fsops,
	&cd9660_fsops,
	&efihttp_fsops,
	&tftp_fsops,
	&nfs_fsops,
	&gzipfs_fsops,
	&bzipfs_fsops,
	NULL
};

struct netif_driver *netif_drivers[] = {
#if defined(LOADER_NET_SUPPORT)
	&efinetif,
#endif
	NULL
};

extern struct console efi_console;
extern struct console eficom;
#if defined(__aarch64__) && __FreeBSD_version < 1500000
/* Hack for backward compatibility -- but only for a while */
extern struct console comconsole;
#endif
#if defined(__amd64__)
extern struct console comconsole;
extern struct console nullconsole;
extern struct console spinconsole;
#endif

struct console *consoles[] = {
	&efi_console,
	&eficom,
#if defined(__aarch64__) && __FreeBSD_version < 1500000
	&comconsole,
#endif
#if defined(__amd64__)
	&comconsole,
	&nullconsole,
	&spinconsole,
#endif
	NULL
};
