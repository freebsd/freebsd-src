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
 * Opaque structures
 */
struct mount;
struct nameidata;
struct proc;
struct sbuf;
struct statfs;
struct thread;
struct uio;
struct vfsconf;
struct vnode;

/*
 * Limits and constants
 */
#define PFS_NAMELEN		24
#define PFS_FSNAMELEN		16	/* equal to MFSNAMELEN */
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
 * Flags
 */
#define PFS_RD		0x0001	/* readable */
#define PFS_WR		0x0002	/* writeable */
#define PFS_RDWR	(PFS_RD|PFS_WR)
#define PFS_RAWRD	0x0004	/* raw reader */
#define	PFS_RAWWR	0x0008	/* raw writer */
#define PFS_RAW		(PFS_RAWRD|PFS_RAWWR)
#define PFS_PROCDEP	0x0010	/* process-dependent */
#define PFS_DISABLED	0x8000	/* node is disabled */

/*
 * Data structures
 */
struct pfs_info;
struct pfs_node;
struct pfs_bitmap;

/*
 * Init / uninit callback
 */
#define PFS_INIT_ARGS \
	struct pfs_info *pi, struct vfsconf *vfc
#define PFS_INIT_PROTO(name) \
	int name(PFS_INIT_ARGS);
typedef int (*pfs_init_t)(PFS_INIT_ARGS);

/*
 * Filler callback
 */
#define PFS_FILL_ARGS \
	struct thread *td, struct proc *p, struct pfs_node *pn, \
	struct sbuf *sb, struct uio *uio
#define PFS_FILL_PROTO(name) \
	int name(PFS_FILL_ARGS);
typedef int (*pfs_fill_t)(PFS_FILL_ARGS);

/*
 * Attribute callback
 */
struct vattr;
#define PFS_ATTR_ARGS \
	struct thread *td, struct proc *p, struct pfs_node *pn, \
	struct vattr *vap
#define PFS_ATTR_PROTO(name) \
	int name(PFS_ATTR_ARGS);
typedef int (*pfs_attr_t)(PFS_ATTR_ARGS);

struct pfs_bitmap;		/* opaque */

/*
 * Visibility callback
 */
#define PFS_VIS_ARGS \
	struct thread *td, struct proc *p, struct pfs_node *pn
#define PFS_VIS_PROTO(name) \
	int name(PFS_VIS_ARGS);
typedef int (*pfs_vis_t)(PFS_VIS_ARGS);

/*
 * Ioctl callback
 */
#define PFS_IOCTL_ARGS \
	struct thread *td, struct proc *p, struct pfs_node *pn, \
	unsigned long cmd, caddr_t data
#define PFS_IOCTL_PROTO(name) \
	int name(PFS_IOCTL_ARGS);
typedef int (*pfs_ioctl_t)(PFS_IOCTL_ARGS);

/*
 * pfs_info: describes a pseudofs instance
 */
struct pfs_info {
	char			 pi_name[PFS_FSNAMELEN];
	pfs_init_t		 pi_init;
	pfs_init_t		 pi_uninit;
	/* members below this line aren't initialized */
	struct pfs_node		*pi_root;
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
	union {
		void		*_pn_dummy;
		pfs_fill_t	 _pn_func;
		struct pfs_node	*_pn_nodes;
	} u1;
#define pn_func		u1._pn_func
#define pn_nodes	u1._pn_nodes
	pfs_ioctl_t		 pn_ioctl;
	pfs_attr_t		 pn_attr;
	pfs_vis_t		 pn_vis;
	void			*pn_data;
	int			 pn_flags;
	
	struct pfs_info		*pn_info;
	struct pfs_node		*pn_parent;
	struct pfs_node		*pn_next;
	u_int32_t		 pn_fileno;
};

/*
 * VFS interface
 */
int	 	 pfs_mount	(struct pfs_info *pi,
				 struct mount *mp, char *path, caddr_t data,
				 struct nameidata *ndp, struct thread *td);
int	 	 pfs_unmount	(struct mount *mp, int mntflags,
				 struct thread *td);
int		 pfs_root	(struct mount *mp, struct vnode **vpp);
int	 	 pfs_statfs	(struct mount *mp, struct statfs *sbp,
				 struct thread *td);
int	 	 pfs_init	(struct pfs_info *pi, struct vfsconf *vfc);
int	 	 pfs_uninit	(struct pfs_info *pi, struct vfsconf *vfc);

/*
 * Directory structure construction and manipulation
 */
struct pfs_node	*pfs_create_dir	(struct pfs_node *parent, char *name,
				 pfs_attr_t attr, pfs_vis_t vis, int flags);
struct pfs_node	*pfs_create_file(struct pfs_node *parent, char *name,
				 pfs_fill_t fill, pfs_attr_t attr,
				 pfs_vis_t vis, int flags);
struct pfs_node	*pfs_create_link(struct pfs_node *parent, char *name,
				 pfs_fill_t fill, pfs_attr_t attr,
				 pfs_vis_t vis, int flags);
int		 pfs_disable	(struct pfs_node *pn);
int		 pfs_enable	(struct pfs_node *pn);
int		 pfs_destroy	(struct pfs_node *pn);

/*
 * Now for some initialization magic...
 */
#define PSEUDOFS(name, version)						\
									\
static struct pfs_info name##_info = {					\
	#name,								\
	&name##_init,							\
	&name##_uninit,							\
};									\
									\
static int								\
_##name##_mount(struct mount *mp, char *path, caddr_t data,		\
	     struct nameidata *ndp, struct thread *td) {		\
        return pfs_mount(&name##_info, mp, path, data, ndp, td);	\
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
static struct vfsops name##_vfsops = {					\
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
MODULE_VERSION(name, version);						\
MODULE_DEPEND(name, pseudofs, 3, 3, 3);

#endif
