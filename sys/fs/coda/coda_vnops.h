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
 * 	@(#) src/sys/coda/coda_vnops.h,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $
 * $FreeBSD$
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
 * This code was written for the Coda file system at Carnegie Mellon
 * University.  Contributers include David Steere, James Kistler, and
 * M. Satyanarayanan.  
 */


/* NetBSD interfaces to the vnodeops */
int coda_open      __P((void *));
int coda_close     __P((void *));
int coda_read      __P((void *));
int coda_write     __P((void *));
int coda_ioctl     __P((void *));
/* 1.3 int cfs_select    __P((void *));*/
int coda_getattr   __P((void *));
int coda_setattr   __P((void *));
int coda_access    __P((void *));
int coda_abortop   __P((void *));
int coda_readlink  __P((void *));
int coda_fsync     __P((void *));
int coda_inactive  __P((void *));
int coda_lookup    __P((void *));
int coda_create    __P((void *));
int coda_remove    __P((void *));
int coda_link      __P((void *));
int coda_rename    __P((void *));
int coda_mkdir     __P((void *));
int coda_rmdir     __P((void *));
int coda_symlink   __P((void *));
int coda_readdir   __P((void *));
int coda_bmap      __P((void *));
int coda_strategy  __P((void *));
int coda_reclaim   __P((void *));
int coda_lock      __P((void *));
int coda_unlock    __P((void *));
int coda_islocked  __P((void *));
int coda_vop_error   __P((void *));
int coda_vop_nop     __P((void *));
int coda_fbsd_getpages	__P((void *));

int coda_rdwr(struct vnode *vp, struct uio *uiop, enum uio_rw rw,
    int ioflag, struct ucred *cred, struct thread *td);
int coda_grab_vnode(dev_t dev, ino_t ino, struct vnode **vpp);
void print_vattr(struct vattr *attr);
void print_cred(struct ucred *cred);
