/*
 * Copyright (c) 2001 Jonathan Lemon <jlemon@freebsd.org>
 * Copyright (c) 2000 Dag-Erling Coïdan Smørgrav
 * Copyright (c) 1999 Pierre Beyssac
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs.h	8.9 (Berkeley) 5/14/95
 *
 * $FreeBSD$
 */

struct pfsnode;

typedef int	node_action_t __P((struct proc *curp, struct proc *p,
		    struct pfsnode *pfs, struct uio *uio));

struct node_data {
	char		*nd_name;
	u_char		nd_namlen;
	u_char		nd_type;
	u_short		nd_mode;
	int		nd_flags;
	node_action_t	*nd_action;
};

/*
 * flag bits for nd_flags.
 */
#define PDEP	0x01			/* entry is process-dependent */

/*
 * control data for the proc file system.
 */
struct pfsnode {
	struct 	pfsnode	*pfs_next;	/* next on list */
	struct 	vnode *pfs_vnode;	/* vnode associated with this pfsnode */
	struct 	node_data *pfs_nd;	/* static initializer */
	pid_t	pfs_pid;		/* associated process */
	u_long	pfs_flags;		/* open flags */
	u_long	pfs_fileno;		/* unique file id */
	pid_t	pfs_lockowner;		/* pfs lock owner */
};

#define PROCFS_NAMELEN 	8	/* max length of a filename component */

/*
 * Kernel stuff follows
 */
#ifdef _KERNEL
#define KMEM_GROUP 2

#define PROCFS_FILENO(nd, pid) 						\
	((nd)->nd_flags & PDEP) ?					\
	    (((pid) + 1) << 4) | ((((u_long)(nd)) >> 3) & 0x0f) :	\
	    (u_long)((nd)->nd_action)

/*
 * Convert between pfsnode vnode
 */
#define VTOPFS(vp)	((struct pfsnode *)(vp)->v_data)
#define PFSTOV(pfs)	((pfs)->pfs_vnode)

typedef struct vfs_namemap vfs_namemap_t;
struct vfs_namemap {
	const char *nm_name;
	int nm_val;
};

int vfs_getuserstr __P((struct uio *, char *, int *));
vfs_namemap_t *vfs_findname __P((vfs_namemap_t *, char *, int));

/* <machine/reg.h> */
struct reg;
struct fpreg;
struct dbreg;

#define PFIND(pid) (pfind(pid))

void 	linprocfs_exit __P((struct proc *));
int 	linprocfs_freevp __P((struct vnode *));
int 	linprocfs_allocvp __P((struct mount *, struct vnode **, long, 
	    struct node_data *));

node_action_t	linprocfs_docmdline;
node_action_t	linprocfs_docpuinfo;
node_action_t	linprocfs_dodevices;
node_action_t	linprocfs_doexelink;
node_action_t	linprocfs_domeminfo;
node_action_t	linprocfs_donetdev;
node_action_t	linprocfs_doprocstat;
node_action_t	linprocfs_doprocstatus;
node_action_t	linprocfs_doselflink;
node_action_t	linprocfs_dostat;
node_action_t	linprocfs_douptime;
node_action_t	linprocfs_doversion;
node_action_t	linprocfs_doloadavg;

extern node_action_t procfs_domem;
extern node_action_t procfs_docmdline;

extern struct node_data root_dir[];

/* functions to check whether or not files should be displayed */
int linprocfs_validfile __P((struct proc *));

#define PROCFS_LOCKED	0x01
#define PROCFS_WANT	0x02

extern vop_t **linprocfs_vnodeop_p;

int	linprocfs_root __P((struct mount *, struct vnode **));
int	linprocfs_rw __P((struct vop_read_args *));
#endif /* _KERNEL */
