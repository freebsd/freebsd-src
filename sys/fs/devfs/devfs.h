/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2000
 *	Poul-Henning Kamp.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the University nor the names of its contributors
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
 *	@(#)kernfs.h	8.6 (Berkeley) 3/29/95
 * From: FreeBSD: src/sys/miscfs/kernfs/kernfs.h 1.14
 *
 * $FreeBSD$
 */

#ifdef _KERNEL

#ifdef DEVFS_INTERN

#define NDEVINO 1024

MALLOC_DECLARE(M_DEVFS);

struct devfs_dirent {
	int	de_inode;
	int	de_flags;
#define	DE_ORPHAN	0x1
#define	DE_DOT		0x2
#define	DE_DOTDOT	0x4
	struct dirent *de_dirent;
	TAILQ_ENTRY(devfs_dirent) de_list;
	TAILQ_HEAD(, devfs_dirent) de_dlist;
	struct devfs_dirent *de_dir;
	int	de_links;
	mode_t	de_mode;
	uid_t	de_uid;
	gid_t	de_gid;
	struct timespec de_atime;
	struct timespec de_mtime;
	struct timespec de_ctime;
	struct vnode *de_vnode;
	char *	de_symlink;
};

struct devfs_node {
	struct kern_target *kf_kt;
};

struct devfs_mount {
	struct vnode	*dm_root;	/* Root node */
	struct devfs_dirent *dm_rootdir;
	struct devfs_dirent *dm_basedir;
	unsigned	dm_generation;
	struct devfs_dirent *dm_dirent[NDEVINO];
#define DE_DELETED ((struct devfs_dirent *)&devfs_inot[0])
	int	dm_inode;
};


extern dev_t devfs_inot[NDEVINO];
extern int devfs_nino;
extern unsigned devfs_generation;


#define VFSTODEVFS(mp)	((struct devfs_mount *)((mp)->mnt_data))

extern vop_t **devfs_vnodeop_p;
extern vop_t **devfs_specop_p;

int devfs_allocv __P((struct devfs_dirent *de, struct mount *mp, struct vnode **vpp, struct proc *p));
struct devfs_dirent * devfs_find __P((struct devfs_dirent *dd, const char *name, int namelen));
int devfs_populate __P((struct devfs_mount *dm));
struct devfs_dirent * devfs_newdirent __P((char *name, int namelen));
void devfs_purge __P((struct devfs_dirent *dd));
struct devfs_dirent * devfs_vmkdir __P((char *name, int namelen,
    struct devfs_dirent *dotdot));
#endif /* DEVFS_INTERN */
#endif /* _KERNEL */
