/*
 * Copyright (c) 1989, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vfs_conf.c	8.11 (Berkeley) 5/10/95
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/vnode.h>

/*
 * These define the root filesystem, device, and root filesystem type.
 */
struct mount *rootfs;
struct vnode *rootvnode;
int (*mountroot)() = NULL;

/*
 * Set up the initial array of known filesystem types.
 */
extern	struct vfsops ufs_vfsops;
extern	int ffs_mountroot();
extern	struct vfsops lfs_vfsops;
extern	int lfs_mountroot();
extern	struct vfsops mfs_vfsops;
extern	int mfs_mountroot();
extern	struct vfsops cd9660_vfsops;
extern	int cd9660_mountroot();
extern	struct vfsops msdos_vfsops;
extern	struct vfsops adosfs_vfsops;
extern	struct vfsops nfs_vfsops;
extern	int nfs_mountroot();
extern	struct vfsops afs_vfsops;
extern	struct vfsops procfs_vfsops;
extern	struct vfsops null_vfsops;
extern	struct vfsops union_vfsops;
extern	struct vfsops umap_vfsops;
extern	struct vfsops portal_vfsops;
extern	struct vfsops fdesc_vfsops;
extern	struct vfsops kernfs_vfsops;

/*
 * Set up the filesystem operations for vnodes.
 */
static struct vfsconf vfsconflist[] = {

	/* Fast Filesystem */
#ifdef FFS
	{ &ufs_vfsops, "ufs", 1, 0, MNT_LOCAL, ffs_mountroot, NULL },
#endif

	/* Log-based Filesystem */
#ifdef LFS
	{ &lfs_vfsops, "lfs", 5, 0, MNT_LOCAL, lfs_mountroot, NULL },
#endif

	/* Memory-based Filesystem */
#ifdef MFS
	{ &mfs_vfsops, "mfs", 3, 0, MNT_LOCAL, mfs_mountroot, NULL },
#endif

	/* ISO9660 (aka CDROM) Filesystem */
#ifdef CD9660
	{ &cd9660_vfsops, "cd9660", 14, 0, MNT_LOCAL, cd9660_mountroot, NULL },
#endif

	/* MSDOS Filesystem */
#ifdef MSDOS
	{ &msdos_vfsops, "msdos", 4, 0, MNT_LOCAL, NULL, NULL },
#endif

	/* AmigaDOS Filesystem */
#ifdef ADOSFS
	{ &adosfs_vfsops, "adosfs", 16, 0, MNT_LOCAL, NULL, NULL },
#endif

	/* Sun-compatible Network Filesystem */
#ifdef NFS
	{ &nfs_vfsops, "nfs", 2, 0, 0, nfs_mountroot, NULL },
#endif

	/* Andrew Filesystem */
#ifdef AFS
	{ &afs_vfsops, "andrewfs", 13, 0, 0, afs_mountroot, NULL },
#endif

	/* /proc Filesystem */
#ifdef PROCFS
	{ &procfs_vfsops, "procfs", 12, 0, 0, NULL, NULL },
#endif

	/* Loopback (Minimal) Filesystem Layer */
#ifdef NULLFS
	{ &null_vfsops, "loopback", 9, 0, 0, NULL, NULL },
#endif

	/* Union (translucent) Filesystem */
#ifdef UNION
	{ &union_vfsops, "union", 15, 0, 0, NULL, NULL },
#endif

	/* User/Group Identifer Remapping Filesystem */
#ifdef UMAPFS
	{ &umap_vfsops, "umap", 10, 0, 0, NULL, NULL },
#endif

	/* Portal Filesystem */
#ifdef PORTAL
	{ &portal_vfsops, "portal", 8, 0, 0, NULL, NULL },
#endif

	/* File Descriptor Filesystem */
#ifdef FDESC
	{ &fdesc_vfsops, "fdesc", 7, 0, 0, NULL, NULL },
#endif

	/* Kernel Information Filesystem */
#ifdef KERNFS
	{ &kernfs_vfsops, "kernfs", 11, 0, 0, NULL, NULL },
#endif

};

/*
 * Initially the size of the list, vfs_init will set maxvfsconf
 * to the highest defined type number.
 */
int maxvfsconf = sizeof(vfsconflist) / sizeof (struct vfsconf);
struct vfsconf *vfsconf = vfsconflist;

/*
 *
 * vfs_opv_descs enumerates the list of vnode classes, each with it's own
 * vnode operation vector.  It is consulted at system boot to build operation
 * vectors.  It is NULL terminated.
 *
 */
extern struct vnodeopv_desc ffs_vnodeop_opv_desc;
extern struct vnodeopv_desc ffs_specop_opv_desc;
extern struct vnodeopv_desc ffs_fifoop_opv_desc;
extern struct vnodeopv_desc lfs_vnodeop_opv_desc;
extern struct vnodeopv_desc lfs_specop_opv_desc;
extern struct vnodeopv_desc lfs_fifoop_opv_desc;
extern struct vnodeopv_desc mfs_vnodeop_opv_desc;
extern struct vnodeopv_desc dead_vnodeop_opv_desc;
extern struct vnodeopv_desc fifo_vnodeop_opv_desc;
extern struct vnodeopv_desc spec_vnodeop_opv_desc;
extern struct vnodeopv_desc nfsv2_vnodeop_opv_desc;
extern struct vnodeopv_desc spec_nfsv2nodeop_opv_desc;
extern struct vnodeopv_desc fifo_nfsv2nodeop_opv_desc;
extern struct vnodeopv_desc fdesc_vnodeop_opv_desc;
extern struct vnodeopv_desc portal_vnodeop_opv_desc;
extern struct vnodeopv_desc null_vnodeop_opv_desc;
extern struct vnodeopv_desc umap_vnodeop_opv_desc;
extern struct vnodeopv_desc kernfs_vnodeop_opv_desc;
extern struct vnodeopv_desc procfs_vnodeop_opv_desc;
extern struct vnodeopv_desc cd9660_vnodeop_opv_desc;
extern struct vnodeopv_desc cd9660_specop_opv_desc;
extern struct vnodeopv_desc cd9660_fifoop_opv_desc;
extern struct vnodeopv_desc union_vnodeop_opv_desc;

struct vnodeopv_desc *vfs_opv_descs[] = {
	&ffs_vnodeop_opv_desc,
	&ffs_specop_opv_desc,
#ifdef FIFO
	&ffs_fifoop_opv_desc,
#endif
	&dead_vnodeop_opv_desc,
#ifdef FIFO
	&fifo_vnodeop_opv_desc,
#endif
	&spec_vnodeop_opv_desc,
#ifdef LFS
	&lfs_vnodeop_opv_desc,
	&lfs_specop_opv_desc,
#ifdef FIFO
	&lfs_fifoop_opv_desc,
#endif
#endif
#ifdef MFS
	&mfs_vnodeop_opv_desc,
#endif
#ifdef NFS
	&nfsv2_vnodeop_opv_desc,
	&spec_nfsv2nodeop_opv_desc,
#ifdef FIFO
	&fifo_nfsv2nodeop_opv_desc,
#endif
#endif
#ifdef FDESC
	&fdesc_vnodeop_opv_desc,
#endif
#ifdef PORTAL
	&portal_vnodeop_opv_desc,
#endif
#ifdef NULLFS
	&null_vnodeop_opv_desc,
#endif
#ifdef UMAPFS
	&umap_vnodeop_opv_desc,
#endif
#ifdef KERNFS
	&kernfs_vnodeop_opv_desc,
#endif
#ifdef PROCFS
	&procfs_vnodeop_opv_desc,
#endif
#ifdef CD9660
	&cd9660_vnodeop_opv_desc,
	&cd9660_specop_opv_desc,
#ifdef FIFO
	&cd9660_fifoop_opv_desc,
#endif
#endif
#ifdef UNION
	&union_vnodeop_opv_desc,
#endif
	NULL
};
