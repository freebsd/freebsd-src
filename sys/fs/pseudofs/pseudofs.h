/*-
 * Copyright (c) 2001 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 *      $FreeBSD$
 */

#ifndef _PSEUDOFS_H_INCLUDED
#define _PSEUDOFS_H_INCLUDED

/*
 * Limits and constants
 */
#define PFS_NAMELEN		24
#define PFS_DELEN		(8 + PFS_NAMELEN)

typedef enum {
	pfstype_none = 0,
	pfstype_root,
	pfstype_dir,
	pfstype_this,
	pfstype_parent,
	pfstype_file,
	pfstype_symlink,
	pfstype_procdir
} pfs_type_t;

/*
 * Data structures
 */
struct pfs_info;
struct pfs_node;
struct pfs_bitmap;

#define PFS_FILL_ARGS \
	struct proc *curp, struct proc *p, struct pfs_node *pn, struct sbuf *sb
#define PFS_FILL_PROTO(name) \
	int name(PFS_FILL_ARGS);
typedef int (*pfs_fill_t)(PFS_FILL_ARGS);

struct pfs_bitmap;		/* opaque */

/*
 * pfs_info: describes a pseudofs instance
 */
struct pfs_info {
	char			 pi_name[MFSNAMELEN];
	struct pfs_node		*pi_root;
	/* members below this line aren't initialized */
	/* currently, the mutex is only used to protect the bitmap */
	struct mtx		 pi_mutex;
	struct pfs_bitmap	*pi_bitmap;
};

/*
 * pfs_node: describes a node (file or directory) within a pseudofs
 */
struct pfs_node {
	char			 pn_name[PFS_NAMELEN];
	pfs_type_t		 pn_type;
	int			 pn_flags;
	uid_t			 pn_uid;
	gid_t			 pn_gid;
	mode_t			 pn_mode;
	union {
		void		*_pn_data;
		pfs_fill_t	 _pn_func;
		struct pfs_node	*_pn_nodes;
	} u1;
#define pn_data		u1._pn_data
#define pn_func		u1._pn_func
#define pn_nodes	u1._pn_nodes
	/* members below this line aren't initialized */
	struct pfs_node		*pn_parent;
	u_int32_t		 pn_fileno;
};

#define PFS_NODE(name, type, flags, uid, gid, mode, data) \
        { (name), (type), (flags), (uid), (gid), (mode), { (data) } }
#define PFS_DIR(name, flags, uid, gid, mode, nodes) \
        PFS_NODE(name, pfstype_dir, flags, uid, gid, mode, nodes)
#define PFS_ROOT(nodes) \
        PFS_NODE("/", pfstype_root, 0, 0, 0, 0555, nodes)
#define PFS_THIS \
	PFS_NODE(".", pfstype_this, 0, 0, 0, 0, NULL)
#define PFS_PARENT \
	PFS_NODE("..", pfstype_parent, 0, 0, 0, 0, NULL)
#define PFS_FILE(name, flags, uid, gid, mode, func) \
	PFS_NODE(name, pfstype_file, flags, uid, gid, mode, func)
#define PFS_SYMLINK(name, flags, uid, gid, mode, func) \
	PFS_NODE(name, pfstype_symlink, flags, uid, gid, mode, func)
#define PFS_PROCDIR(flags, uid, gid, mode, nodes) \
        PFS_NODE("", pfstype_procdir, flags, 0, uid, gid, mode, nodes)
#define PFS_LASTNODE \
	PFS_NODE("", pfstype_none, 0, 0, 0, 0, NULL)

/*
 * VFS interface
 */
int	 pfs_mount		(struct pfs_info *pi,
				 struct mount *mp, char *path, caddr_t data,
				 struct nameidata *ndp, struct proc *p);
int	 pfs_unmount		(struct mount *mp, int mntflags,
				 struct proc *p);
int	 pfs_root		(struct mount *mp, struct vnode **vpp);
int	 pfs_statfs		(struct mount *mp, struct statfs *sbp,
				 struct proc *p);
int	 pfs_init		(struct pfs_info *pi, struct vfsconf *vfc);
int	 pfs_uninit		(struct pfs_info *pi, struct vfsconf *vfc);

/*
 * Now for some initialization magic...
 */
#define PSEUDOFS(name, root)						\
									\
static struct pfs_info name##_info = {					\
	#name,								\
	&(root)								\
};									\
									\
static int								\
_##name##_mount(struct mount *mp, char *path, caddr_t data,		\
	     struct nameidata *ndp, struct proc *p) {			\
        return pfs_mount(&name##_info, mp, path, data, ndp, p);		\
}									\
									\
static int								\
_##name##_init(struct vfsconf *vfc) {					\
        return pfs_init(&name##_info, vfc);				\
}									\
									\
static int								\
_##name##_uninit(struct vfsconf *vfc) {					\
        return pfs_uninit(&name##_info, vfc);				\
}									\
									\
static struct vfsops testfs_vfsops = {					\
	_##name##_mount,						\
	vfs_stdstart,							\
	pfs_unmount,							\
	pfs_root,							\
	vfs_stdquotactl,						\
	pfs_statfs,							\
	vfs_stdsync,							\
	vfs_stdvget,							\
	vfs_stdfhtovp,							\
	vfs_stdcheckexp,						\
	vfs_stdvptofh,							\
	_##name##_init,							\
	_##name##_uninit,						\
	vfs_stdextattrctl,						\
};									\
VFS_SET(name##_vfsops, name, VFCF_SYNTHETIC);				\
MODULE_DEPEND(name, pseudofs, 1, 1, 1);

#endif
