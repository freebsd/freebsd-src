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
 * 	@(#) src/sys/coda/coda_vnops.h,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $
 * $FreeBSD$
  */

/*
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda filesystem at Carnegie Mellon
 * University.  Contributers include David Steere, James Kistler, and
 * M. Satyanarayanan.
 */

#ifndef _CODA_VNOPS_H_
#define	_CODA_VNOPS_H_

/*
 * FreeBSD interfaces to the vnodeops.
 */
vop_open_t	coda_open;
vop_close_t	coda_close;
vop_read_t	coda_read;
vop_write_t	coda_write;
vop_ioctl_t	coda_ioctl;
vop_getattr_t	coda_getattr;
vop_setattr_t	coda_setattr;
vop_access_t	coda_access;
vop_readlink_t	coda_readlink;
vop_fsync_t	coda_fsync;
vop_inactive_t	coda_inactive;
vop_lookup_t	coda_lookup;
vop_create_t	coda_create;
vop_remove_t	coda_remove;
vop_link_t	coda_link;
vop_rename_t	coda_rename;
vop_mkdir_t	coda_mkdir;
vop_rmdir_t	coda_rmdir;
vop_symlink_t	coda_symlink;
vop_readdir_t	coda_readdir;
vop_strategy_t	coda_strategy;
vop_reclaim_t	coda_reclaim;
vop_lock1_t	coda_lock;
vop_unlock_t	coda_unlock;
vop_islocked_t	coda_islocked;
vop_pathconf_t	coda_pathconf;

int	coda_rdwr(struct vnode *vp, struct uio *uiop, enum uio_rw rw,
	    int ioflag, struct ucred *cred, struct thread *td);
void	coda_print_cred(struct ucred *cred);

#endif /* !_CODA_VNOPS_H_ */
