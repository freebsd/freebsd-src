/*

            Coda: an Experimental Distributed File System
                             Release 3.1

          Copyright (c) 1987-1998 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

/* $Header: /afs/cs/project/coda-src/cvs/coda/kernel-src/vfs/freebsd/cfs/cfs_vfsops.h,v 1.9 1998/08/28 18:12:22 rvb Exp $ */

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

int cfs_vfsopstats_init(void);
#ifdef	NetBSD1_3
int cfs_mount(struct mount *, const char *, void *, struct nameidata *, 
		       struct proc *);
#else
int cfs_mount(struct mount *, char *, caddr_t, struct nameidata *, 
		       struct proc *);
#endif
int cfs_start(struct mount *, int, struct proc *);
int cfs_unmount(struct mount *, int, struct proc *);
int cfs_root(struct mount *, struct vnode **);
int cfs_quotactl(struct mount *, int, uid_t, caddr_t, struct proc *);
int cfs_nb_statfs(struct mount *, struct statfs *, struct proc *);
int cfs_sync(struct mount *, int, struct ucred *, struct proc *);
int cfs_vget(struct mount *, ino_t, struct vnode **);
int cfs_fhtovp(struct mount *, struct fid *, struct mbuf *, struct vnode **,
		       int *, struct ucred **);
int cfs_vptofh(struct vnode *, struct fid *);
#ifdef	__NetBSD__
void cfs_init(void);
#elif defined(__FreeBSD__)
#ifdef	__FreeBSD_version
int cfs_init(struct vfsconf *vfsp);
#else
int cfs_init(void);
#endif
#endif
#if	defined(__NetBSD__) && defined(NetBSD1_3) && (NetBSD1_3 >= 7)
int cfs_sysctl(int *, u_int, void *, size_t *, void *, size_t,
		    struct proc *);
#endif
int getNewVnode(struct vnode **vpp);
