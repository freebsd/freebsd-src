/*
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
 *	@(#)procfs.h	8.6 (Berkeley) 2/3/94
 *
 *	$Id: procfs.h,v 1.12 1996/07/02 13:38:07 dyson Exp $
 */

/*
 * The different types of node in a procfs filesystem
 */
typedef enum {
	Proot,		/* the filesystem root */
	Pproc,		/* a process-specific sub-directory */
	Pfile,		/* the executable file */
	Pmem,		/* the process's memory image */
	Pregs,		/* the process's register set */
	Pfpregs,	/* the process's FP register set */
	Pctl,		/* process control */
	Pstatus,	/* process status */
	Pnote,		/* process notifier */
	Pnotepg,	/* process group notifier */
	Pmap,		/* memory map */
	Ptype		/* executable type */
} pfstype;

/*
 * control data for the proc file system.
 */
struct pfsnode {
	struct pfsnode	*pfs_next;	/* next on list */
	struct vnode	*pfs_vnode;	/* vnode associated with this pfsnode */
	pfstype		pfs_type;	/* type of procfs node */
	pid_t		pfs_pid;	/* associated process */
	u_short		pfs_mode;	/* mode bits for stat() */
	u_long		pfs_flags;	/* open flags */
	u_long		pfs_fileno;	/* unique file id */
	pid_t		pfs_lockowner;	/* pfs lock owner */
};

#define PROCFS_NOTELEN	64	/* max length of a note (/proc/$pid/note) */
#define PROCFS_CTLLEN 	8	/* max length of a ctl msg (/proc/$pid/ctl */

/*
 * Kernel stuff follows
 */
#ifdef KERNEL
#define CNEQ(cnp, s, len) \
	 ((cnp)->cn_namelen == (len) && \
	  (bcmp((s), (cnp)->cn_nameptr, (len)) == 0))

#define KMEM_GROUP 2

/*
 * Check to see whether access to target process is allowed
 * Evaluates to 1 if access is allowed.
 */
#define CHECKIO(p1, p2) \
     ((((p1)->p_cred->pc_ucred->cr_uid == (p2)->p_cred->p_ruid) && \
       ((p1)->p_cred->p_ruid == (p2)->p_cred->p_ruid) && \
       ((p1)->p_cred->p_svuid == (p2)->p_cred->p_ruid) && \
       ((p2)->p_flag & P_SUGID) == 0) || \
      (suser((p1)->p_cred->pc_ucred, &(p1)->p_acflag) == 0))
      
/*
 * Format of a directory entry in /proc, ...
 * This must map onto struct dirent (see <dirent.h>)
 */
#define PROCFS_NAMELEN 8
struct pfsdent {
	u_long	d_fileno;
	u_short	d_reclen;
	u_char	d_type;
	u_char	d_namlen;
	char	d_name[PROCFS_NAMELEN];
};
#define UIO_MX sizeof(struct pfsdent)
#define PROCFS_FILENO(pid, type) \
	(((type) == Proot) ? \
			2 : \
			((((pid)+1) << 3) + ((int) (type))))

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

extern int vfs_getuserstr __P((struct uio *, char *, int *));
extern vfs_namemap_t *vfs_findname __P((vfs_namemap_t *, char *, int));

/* <machine/reg.h> */
struct reg;
struct fpreg;

#define PFIND(pid) ((pid) ? pfind(pid) : &proc0)
extern int procfs_freevp __P((struct vnode *));
extern int procfs_allocvp __P((struct mount *, struct vnode **, long, pfstype));
extern struct vnode *procfs_findtextvp __P((struct proc *));
extern int procfs_sstep __P((struct proc *));
extern void procfs_fix_sstep __P((struct proc *));
extern int procfs_read_regs __P((struct proc *, struct reg *));
extern int procfs_write_regs __P((struct proc *, struct reg *));
extern int procfs_read_fpregs __P((struct proc *, struct fpreg *));
extern int procfs_write_fpregs __P((struct proc *, struct fpreg *));
extern int procfs_donote __P((struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio));
extern int procfs_doregs __P((struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio));
extern int procfs_dofpregs __P((struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio));
extern int procfs_domem __P((struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio));
extern int procfs_doctl __P((struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio));
extern int procfs_dostatus __P((struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio));
extern int procfs_domap __P((struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio));
extern int procfs_dotype __P((struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio));

/* check to see if the process has the "items" (regs/file) */
int procfs_validfile __P((struct proc *));
int procfs_validfpregs __P((struct proc *));
int procfs_validregs __P((struct proc *));
int procfs_validmap __P((struct proc *));
int procfs_validtype __P((struct proc *));

#define PROCFS_LOCKED	0x01
#define PROCFS_WANT	0x02

extern vop_t **procfs_vnodeop_p;
extern struct vfsops procfs_vfsops;

int	procfs_root __P((struct mount *, struct vnode **));
int	procfs_rw __P((struct vop_read_args *));
#endif /* KERNEL */
