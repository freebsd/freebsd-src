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

/* $Header: /afs/cs/project/coda-src/cvs/coda/kernel-src/vfs/freebsd/cfs/cnode.h,v 1.10 1998/08/28 18:12:25 rvb Exp $ */

/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda file system at Carnegie Mellon University.
 * Contributers include David Steere, James Kistler, and M. Satyanarayanan.
 */

/* 
 * HISTORY
 * $Log: cnode.h,v $
 * Revision 1.10  1998/08/28 18:12:25  rvb
 * Now it also works on FreeBSD -current.  This code will be
 * committed to the FreeBSD -current and NetBSD -current
 * trees.  It will then be tailored to the particular platform
 * by flushing conditional code.
 *
 * Revision 1.9  1998/08/18 17:05:24  rvb
 * Don't use __RCSID now
 *
 * Revision 1.8  1998/08/18 16:31:49  rvb
 * Sync the code for NetBSD -current; test on 1.3 later
 *
 * Revision 1.7  98/02/24  22:22:53  rvb
 * Fixes up mainly to flush iopen and friends
 * 
 * Revision 1.6  98/01/31  20:53:19  rvb
 * First version that works on FreeBSD 2.2.5
 * 
 * Revision 1.5  98/01/23  11:53:51  rvb
 * Bring RVB_CFS1_1 to HEAD
 * 
 * Revision 1.4.2.5  98/01/23  11:21:14  rvb
 * Sync with 2.2.5
 * 
 * Revision 1.4.2.4  98/01/22  13:03:38  rvb
 * Had Breaken ls .
 * 
 * Revision 1.4.2.3  97/12/19  14:26:09  rvb
 * session id
 * 
 * Revision 1.4.2.2  97/12/16  12:40:24  rvb
 * Sync with 1.3
 * 
 * Revision 1.4.2.1  97/12/06  17:41:28  rvb
 * Sync with peters coda.h
 * 
 * Revision 1.4  97/12/05  10:39:30  rvb
 * Read CHANGES
 * 
 * Revision 1.3.18.2  97/11/12  12:09:45  rvb
 * reorg pass1
 * 
 * Revision 1.3.18.1  97/10/29  16:06:31  rvb
 * Kill DYING
 * 
 * Revision 1.3  1996/12/12 22:11:03  bnoble
 * Fixed the "downcall invokes venus operation" deadlock in all known cases. 
 *  There may be more.
 *
 * Revision 1.2  1996/01/02 16:57:26  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:57:53  bnoble
 * Added CFS-specific files
 *
 * Revision 3.1.1.1  1995/03/04  19:08:23  bnoble
 * Branch for NetBSD port revisions
 *
 * Revision 3.1  1995/03/04  19:08:23  bnoble
 * Bump to major revision 3 to prepare for NetBSD port
 *
 * Revision 2.2  1994/12/06  13:39:18  dcs
 * Add a flag value to indicate a cnode was orphaned, e.g. the venus
 * that created it has exited. This will allow one to restart venus
 * even though some process may be cd'd into /coda.
 *
 * Revision 2.1  94/07/21  16:25:33  satya
 * Conversion to C++ 3.0; start of Coda Release 2.0
 * 
 * Revision 1.2.7.1  94/06/16  11:26:02  raiff
 * Branch for release beta-16Jun1994_39118
 * 
 * Revision 1.2  92/10/27  17:58:41  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 2.3  92/09/30  14:16:53  mja
 * 	Picked up fixed #ifdef _KERNEL. Also...
 * 
 * 	Substituted rvb's history blurb so that we agree with Mach 2.5 sources.
 * 	[91/02/09            jjk]
 * 
 * 	Added contributors blurb.
 * 	[90/12/13            jjk]
 * 
 * Revision 2.2  90/07/05  11:27:24  mrt
 * 	Created for the Coda File System.
 * 	[90/05/23            dcs]
 * 
 * Revision 1.4  90/05/31  17:02:16  dcs
 * Prepare for merge with facilities kernel.
 * 
 * 
 * 
 */

#ifndef	_CNODE_H_
#define	_CNODE_H_

#include <sys/vnode.h>

#ifdef	__FreeBSD__

/* for the prototype of DELAY() */
#include <machine/clock.h>

#ifdef	__FreeBSD_version
/* You would think that <sys/param.h> or something would include this */
#include <sys/lock.h>

MALLOC_DECLARE(M_CFS);

#else

/* yuck yuck yuck */
#define vref(x) cvref(x)
extern void cvref(struct vnode *vp);
/* yuck yuck yuck */

#endif
#endif

#if	defined(__NetBSD__) && defined(NetBSD1_3) && (NetBSD1_3 >= 7)
#define	NEW_LOCKMGR(l, f, i) lockmgr(l, f, i)
#define	VOP_X_LOCK(vn, fl) vn_lock(vn, fl)
#define	VOP_X_UNLOCK(vn, fl) VOP_UNLOCK(vn, fl)

#elif defined(__FreeBSD_version)
#define	NEW_LOCKMGR(l, f, i) lockmgr(l, f, i, curproc)
#define	VOP_X_LOCK(vn, fl) vn_lock(vn, fl, curproc)
#define	VOP_X_UNLOCK(vn, fl) VOP_UNLOCK(vn, fl, curproc)

/* NetBSD 1.3 & FreeBSD 2.2.x */
#else
#undef	NEW_LOCKMGR
#define	VOP_X_LOCK(vn, fl) VOP_LOCK(vn)
#define	VOP_X_UNLOCK(vn, fl) VOP_UNLOCK(vn)
#endif

/*
 * tmp below since we need struct queue
 */
#include <cfs/cfsk.h>

/*
 * Cnode lookup stuff.
 * NOTE: CFS_CACHESIZE must be a power of 2 for cfshash to work!
 */
#define CFS_CACHESIZE 512

#define CFS_ALLOC(ptr, cast, size)                                        \
do {                                                                      \
    ptr = (cast)malloc((unsigned long) size, M_CFS, M_WAITOK);            \
    if (ptr == 0) {                                                       \
	panic("kernel malloc returns 0 at %s:%d\n", __FILE__, __LINE__);  \
    }                                                                     \
} while (0)

#define CFS_FREE(ptr, size)  free((ptr), M_CFS)

/*
 * global cache state control
 */
extern int cfsnc_use;

/*
 * Used to select debugging statements throughout the cfs code.
 */
extern int cfsdebug;
extern int cfsnc_debug;
extern int cfs_printf_delay;
extern int cfs_vnop_print_entry;
extern int cfs_psdev_print_entry;
extern int cfs_vfsop_print_entry;

#define CFSDBGMSK(N)            (1 << N)
#define CFSDEBUG(N, STMT)       { if (cfsdebug & CFSDBGMSK(N)) { STMT } }
#define myprintf(args)          \
do {                            \
    if (cfs_printf_delay)       \
	DELAY(cfs_printf_delay);\
    printf args ;               \
} while (0)

struct cnode {
    struct vnode	*c_vnode;
    u_short		 c_flags;	/* flags (see below) */
    ViceFid		 c_fid;		/* file handle */
#ifdef	NEW_LOCKMGR
    struct lock		 c_lock;	/* new lock protocol */
#endif
    struct vnode	*c_ovp;		/* open vnode pointer */
    u_short		 c_ocount;	/* count of openers */
    u_short		 c_owrite;	/* count of open for write */
    struct vattr	 c_vattr; 	/* attributes */
    char		*c_symlink;	/* pointer to symbolic link */
    u_short		 c_symlen;	/* length of symbolic link */
    dev_t		 c_device;	/* associated vnode device */
    ino_t		 c_inode;	/* associated vnode inode */
    struct cnode	*c_next;	/* links if on NetBSD machine */
};
#define	VTOC(vp)	((struct cnode *)(vp)->v_data)
#define	CTOV(cp)	((struct vnode *)((cp)->c_vnode))

/* flags */
#define C_VATTR		0x01	/* Validity of vattr in the cnode */
#define C_SYMLINK	0x02	/* Validity of symlink pointer in the Code */
#define C_WANTED	0x08	/* Set if lock wanted */
#define C_LOCKED	0x10	/* Set if lock held */
#define C_UNMOUNTING	0X20	/* Set if unmounting */
#define C_PURGING	0x40	/* Set if purging a fid */

#define VALID_VATTR(cp)		((cp->c_flags) & C_VATTR)
#define VALID_SYMLINK(cp)	((cp->c_flags) & C_SYMLINK)
#define IS_UNMOUNTING(cp)	((cp)->c_flags & C_UNMOUNTING)

struct vcomm {
	u_long		vc_seq;
	struct selinfo	vc_selproc;
	struct queue	vc_requests;
	struct queue	vc_replys;
};

#define	VC_OPEN(vcp)	    ((vcp)->vc_requests.forw != NULL)
#define MARK_VC_CLOSED(vcp) (vcp)->vc_requests.forw = NULL;
#define MARK_VC_OPEN(vcp)    /* MT */

struct cfs_clstat {
	int	ncalls;			/* client requests */
	int	nbadcalls;		/* upcall failures */
	int	reqs[CFS_NCALLS];	/* count of each request */
};
extern struct cfs_clstat cfs_clstat;

/*
 * CFS structure to hold mount/file system information
 */
struct cfs_mntinfo {
    struct vnode	*mi_rootvp;
    struct mount	*mi_vfsp;
    struct vcomm	 mi_vcomm;
};
extern struct cfs_mntinfo cfs_mnttbl[]; /* indexed by minor device number */

/*
 * vfs pointer to mount info
 */
#define vftomi(vfsp)    ((struct cfs_mntinfo *)(vfsp->mnt_data))
#define	CFS_MOUNTED(vfsp)   (vftomi((vfsp)) != (struct cfs_mntinfo *)0)

/*
 * vnode pointer to mount info
 */
#define vtomi(vp)       ((struct cfs_mntinfo *)(vp->v_mount->mnt_data))

/*
 * Used for identifying usage of "Control" object
 */
extern struct vnode *cfs_ctlvp;
#define	IS_CTL_VP(vp)		((vp) == cfs_ctlvp)
#define	IS_CTL_NAME(vp, name, l)((l == CFS_CONTROLLEN) \
 				 && ((vp) == vtomi((vp))->mi_rootvp)    \
				 && strncmp(name, CFS_CONTROL, l) == 0)

/* 
 * An enum to tell us whether something that will remove a reference
 * to a cnode was a downcall or not
 */
enum dc_status {
    IS_DOWNCALL = 6,
    NOT_DOWNCALL = 7
};

/* cfs_psdev.h */
int cfscall(struct cfs_mntinfo *mntinfo, int inSize, int *outSize, caddr_t buffer);

/* cfs_subr.h */
int  handleDownCall(int opcode, union outputArgs *out);
void cfs_unmounting(struct mount *whoIam);
int  cfs_vmflush(struct cnode *cp);

/* cfs_vnodeops.h */
struct cnode *makecfsnode(ViceFid *fid, struct mount *vfsp, short type);
int cfs_vnodeopstats_init(void);

/* cfs_vfsops.h */
struct mount *devtomp(dev_t dev);

#if	!(defined NetBSD1_3) && !defined(__FreeBSD_version)
#define __RCSID(x) static char *rcsid = x
#endif

/* sigh */
#define CFS_RDWR ((u_long) 31)

#endif	/* _CNODE_H_ */

