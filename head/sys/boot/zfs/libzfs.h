/*-
 * Copyright (c) 2012 Andriy Gapon <avg@FreeBSD.org>
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
 * $FreeBSD$
 */

#ifndef _BOOT_LIBZFS_H_
#define _BOOT_LIBZFS_H_

#define	ZFS_MAXNAMELEN	256

/*
 * ZFS fully-qualified device descriptor.
 * Note, this must match the 'struct devdesc' declaration in bootstrap.h.
 * Arch-specific device descriptors should be binary compatible with this
 * structure if they are to support ZFS.
 */
struct zfs_devdesc
{
    struct devsw	*d_dev;
    int			d_type;
    int			d_unit;
    void		*d_opendata;
    uint64_t		pool_guid;
    uint64_t		root_guid;
};

struct zfs_boot_args
{
    uint32_t		size;
    uint32_t		reserved;
    uint64_t		pool;
    uint64_t		root;
    uint64_t		primary_pool;
    uint64_t		primary_vdev;
    char		gelipw[256];
};

int	zfs_parsedev(struct zfs_devdesc *dev, const char *devspec,
		     const char **path);
char	*zfs_fmtdev(void *vdev);
int	zfs_probe_dev(const char *devname, uint64_t *pool_guid);
int	zfs_list(const char *name);
void	init_zfs_bootenv(char *currdev);
int	zfs_bootenv(const char *name);
int	zfs_belist_add(const char *name);
int	zfs_set_env(void);

extern struct devsw zfs_dev;
extern struct fs_ops zfs_fsops;

#endif /*_BOOT_LIBZFS_H_*/
