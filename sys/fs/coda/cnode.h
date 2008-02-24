/*-
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
 * $FreeBSD: src/sys/fs/coda/cnode.h,v 1.21.4.1 2008/01/23 12:09:43 rwatson Exp $
 * 
 */

/*-
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda filesystem at Carnegie Mellon University.
 * Contributers include David Steere, James Kistler, and M. Satyanarayanan.
 */

#ifndef	_CNODE_H_
#define	_CNODE_H_

#include <sys/vnode.h>
#include <sys/lock.h>

MALLOC_DECLARE(M_CODA);

/*
 * tmp below since we need struct queue
 */
#include <fs/coda/coda_kernel.h>

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
 * Used to select debugging statements throughout the cfs code.
 */
extern int codadebug;
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
    CodaFid		 c_fid;		/* file handle */
    struct vnode	*c_ovp;		/* open vnode pointer */
    u_short		 c_ocount;	/* count of openers */
    u_short		 c_owrite;	/* count of open for write */
    struct vattr	 c_vattr; 	/* attributes */
    char		*c_symlink;	/* pointer to symbolic link */
    u_short		 c_symlen;	/* length of symbolic link */
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
 * CODA structure to hold mount/filesystem information
 */
struct coda_mntinfo {
    struct vnode	*mi_rootvp;
    struct mount	*mi_vfsp;
    struct vcomm	 mi_vcomm;
    struct cdev		*dev;
    int                  mi_started;
    LIST_ENTRY(coda_mntinfo) mi_list;
};
struct coda_mntinfo *dev2coda_mntinfo(struct cdev *dev);

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
extern int coda_kernel_version;

/* cfs_subr.h */
int  handleDownCall(int opcode, union outputArgs *out);
void coda_unmounting(struct mount *whoIam);
int  coda_vmflush(struct cnode *cp);

/* cfs_vnodeops.h */
struct cnode *make_coda_node(CodaFid *fid, struct mount *vfsp, short type);
int coda_vnodeopstats_init(void);

/* coda_vfsops.h */
struct mount *devtomp(struct cdev *dev);

/* sigh */
#define CODA_RDWR ((u_long) 31)

#endif	/* _CNODE_H_ */

