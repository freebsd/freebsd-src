/*
 * 
 *             Coda: an Experimental Distributed File System
 *                              Release 3.1
 * 
 *           Copyright (c) 1987-1998 Carnegie Mellon University
 *                          All Rights Reserved
 * 
 * Permission  to  use, copy, modify and distribute this software and its
 * documentation is hereby granted,  provided  that  both  the  copyright
 * notice  and  this  permission  notice  appear  in  all  copies  of the
 * software, derivative works or  modified  versions,  and  any  portions
 * thereof, and that both notices appear in supporting documentation, and
 * that credit is given to Carnegie Mellon University  in  all  documents
 * and publicity pertaining to direct or indirect use of this code or its
 * derivatives.
 * 
 * CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
 * SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
 * FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
 * DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
 * RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
 * ANY DERIVATIVE WORK.
 * 
 * Carnegie  Mellon  encourages  users  of  this  software  to return any
 * improvements or extensions that  they  make,  and  to  grant  Carnegie
 * Mellon the rights to redistribute these changes without encumbrance.
 * 
 * 	@(#) src/sys/cfs/coda_vfsops.h,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $ 
 * $FreeBSD: src/sys/coda/coda_vfsops.h,v 1.4 1999/08/28 00:40:57 peter Exp $
 * 
 */

/*
 * cfid structure:
 * This overlays the fid structure (see vfs.h)
 * Only used below and will probably go away.
 */

struct cfid {
    u_short	cfid_len;
    u_short     padding;
    ViceFid	cfid_fid;
};

struct mount;

int coda_vfsopstats_init(void);
int coda_mount(struct mount *, char *, caddr_t, struct nameidata *, 
		       struct proc *);
int coda_start(struct mount *, int, struct proc *);
int coda_unmount(struct mount *, int, struct proc *);
int coda_root(struct mount *, struct vnode **);
int coda_quotactl(struct mount *, int, uid_t, caddr_t, struct proc *);
int coda_nb_statfs(struct mount *, struct statfs *, struct proc *);
int coda_sync(struct mount *, int, struct ucred *, struct proc *);
int coda_vget(struct mount *, ino_t, struct vnode **);
int coda_fhtovp(struct mount *, struct fid *, struct mbuf *, struct vnode **,
		       int *, struct ucred **);
int coda_vptofh(struct vnode *, struct fid *);
int coda_init(struct vfsconf *vfsp);

int getNewVnode(struct vnode **vpp);
