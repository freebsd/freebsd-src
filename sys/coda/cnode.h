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
 * 	@(#) src/sys/coda/cnode.h,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $ 
 *  $Id: cnode.h,v 1.3 1998/09/11 18:50:17 rvb Exp $
 * 
 */

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
 * Revision 1.3  1998/09/11 18:50:17  rvb
 * All the references to cfs, in symbols, structs, and strings
 * have been changed to coda.  (Same for CFS.)
 *
 * Revision 1.2  1998/09/02 19:09:53  rvb
 * Pass2 complete
 *
 * Revision 1.1.1.1  1998/08/29 21:14:52  rvb
 * Very Preliminary Coda
 *
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
 * Bring RVB_CODA1_1 to HEAD
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
 * Added CODA-specific files
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
#include <sys/lock.h>
#include <machine/clock.h>

MALLOC_DECLARE(M_CODA);

/*
 * tmp below since we need struct queue
 */
#include <coda/coda_kernel.h>

/*
 * Cnode lookup stuff.
 * NOTE: CODA_CACHESIZE must be a power of 2 for cfshash to work!
 */
#define CODA_CACHESIZE 512

#define CODA_ALLOC(ptr, cast, size)                                        \
do {                                                                      \
    ptr = (cast)malloc((unsigned long) size, M_CODA, M_WAITOK);            \
    if (ptr == 0) {                                                       \
	panic("kernel malloc returns 0 at %s:%d\n", __FILE__, __LINE__);  \
    }                                                                     \
} while (0)

#define CODA_FREE(ptr, size)  free((ptr), M_CODA)

/*
 * global cache state control
 */
extern int coda_nc_use;

/*
 * Used to select debugging statements throughout the cfs code.
 */
extern int codadebug;
extern int coda_nc_debug;
extern int coda_printf_delay;
extern int coda_vnop_print_entry;
extern int coda_psdev_print_entry;
extern int coda_vfsop_print_entry;

#define CODADBGMSK(N)            (1 << N)
#define CODADEBUG(N, STMT)       { if (codadebug & CODADBGMSK(N)) { STMT } }
#define myprintf(args)          \
do {                            \
    if (coda_printf_delay)       \
	DELAY(coda_printf_delay);\
    printf args ;               \
} while (0)

struct cnode {
    struct vnode	*c_vnode;
    u_short		 c_flags;	/* flags (see below) */
    ViceFid		 c_fid;		/* file handle */
    struct lock		 c_lock;	/* new lock protocol */
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

struct coda_clstat {
	int	ncalls;			/* client requests */
	int	nbadcalls;		/* upcall failures */
	int	reqs[CODA_NCALLS];	/* count of each request */
};
extern struct coda_clstat coda_clstat;

/*
 * CODA structure to hold mount/file system information
 */
struct coda_mntinfo {
    struct vnode	*mi_rootvp;
    struct mount	*mi_vfsp;
    struct vcomm	 mi_vcomm;
};
extern struct coda_mntinfo coda_mnttbl[]; /* indexed by minor device number */

/*
 * vfs pointer to mount info
 */
#define vftomi(vfsp)    ((struct coda_mntinfo *)(vfsp->mnt_data))
#define	CODA_MOUNTED(vfsp)   (vftomi((vfsp)) != (struct coda_mntinfo *)0)

/*
 * vnode pointer to mount info
 */
#define vtomi(vp)       ((struct coda_mntinfo *)(vp->v_mount->mnt_data))

/*
 * Used for identifying usage of "Control" object
 */
extern struct vnode *coda_ctlvp;
#define	IS_CTL_VP(vp)		((vp) == coda_ctlvp)
#define	IS_CTL_NAME(vp, name, l)((l == CODA_CONTROLLEN) \
 				 && ((vp) == vtomi((vp))->mi_rootvp)    \
				 && strncmp(name, CODA_CONTROL, l) == 0)

/* 
 * An enum to tell us whether something that will remove a reference
 * to a cnode was a downcall or not
 */
enum dc_status {
    IS_DOWNCALL = 6,
    NOT_DOWNCALL = 7
};

/* cfs_psdev.h */
int coda_call(struct coda_mntinfo *mntinfo, int inSize, int *outSize, caddr_t buffer);

/* cfs_subr.h */
int  handleDownCall(int opcode, union outputArgs *out);
void coda_unmounting(struct mount *whoIam);
int  coda_vmflush(struct cnode *cp);

/* cfs_vnodeops.h */
struct cnode *make_coda_node(ViceFid *fid, struct mount *vfsp, short type);
int coda_vnodeopstats_init(void);

/* coda_vfsops.h */
struct mount *devtomp(dev_t dev);

/* sigh */
#define CODA_RDWR ((u_long) 31)

#endif	/* _CNODE_H_ */

