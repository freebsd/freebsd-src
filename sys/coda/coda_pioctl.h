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

/* $Header: /afs/cs/project/coda-src/cvs/coda/kernel-src/vfs/freebsd/cfs/pioctl.h,v 1.7 1998/08/28 18:12:26 rvb Exp $ */

/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * HISTORY
 * $Log: pioctl.h,v $
 * Revision 1.7  1998/08/28 18:12:26  rvb
 * Now it also works on FreeBSD -current.  This code will be
 * committed to the FreeBSD -current and NetBSD -current
 * trees.  It will then be tailored to the particular platform
 * by flushing conditional code.
 *
 * Revision 1.6  1998/08/18 17:05:26  rvb
 * Don't use __RCSID now
 *
 * Revision 1.5  1998/08/18 16:31:51  rvb
 * Sync the code for NetBSD -current; test on 1.3 later
 *
 * Revision 1.4  98/01/23  11:53:54  rvb
 * Bring RVB_CFS1_1 to HEAD
 * 
 * Revision 1.3.2.1  97/12/06  17:41:29  rvb
 * Sync with peters coda.h
 * 
 * Revision 1.3  97/12/05  10:39:31  rvb
 * Read CHANGES
 * 
 * Revision 1.2.34.2  97/11/13  22:03:06  rvb
 * pass2 cfs_NetBSD.h mt
 * 
 * Revision 1.2.34.1  97/11/12  12:38:11  rvb
 * mach_vioctl.h -> pioctl.h
 * 
 * Revision 1.2  96/01/02  16:57:27  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 * 
 * Revision 1.1.2.1  1995/12/20 01:57:54  bnoble
 * Added CFS-specific files
 *
 * Revision 2.4  90/08/30  11:51:12  bohman
 * 	Ioctl changes for STDC.
 * 	[90/08/28            bohman]
 * 
 * Revision 2.3  89/03/09  22:10:26  rpd
 * 	More cleanup.
 * 
 * Revision 2.2  89/02/25  17:58:32  gm0w
 * 	Changes for cleanup.
 * 
 *  7-Feb-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	No need for VICE conditional.
 *
 * 22-Oct-86  Jay Kistler (jjk) at Carnegie-Mellon University
 *	Created from Andrew's vice.h and viceioctl.h.
 *
 */
/*
 * ITC Remote file system - vice ioctl interface module
 */

/*
 *  TODO:  Find /usr/local/include/viceioctl.h.
 */

#ifndef	_SYS_PIOCTL_H_
#define _SYS_PIOCTL_H_

/* The 2K limits above are a consequence of the size of the kernel buffer
   used to buffer requests from the user to venus--2*MAXPATHLEN.
   The buffer pointers may be null, or the counts may be 0 if there
   are no input or output parameters
 */

#ifdef	__STDC__
#define _VICEIOCTL(id)  ((unsigned int ) _IOW('V', id, struct ViceIoctl))
#else
#define _VICEIOCTL(id)  ((unsigned int ) _IOW(V, id, struct ViceIoctl))
#endif
/* Use this macro to define up to 256 vice ioctl's.  These ioctl's
   all potentially have in/out parameters--this depends upon the
   values in the ViceIoctl structure.  This structure is itself passed
   into the kernel by the normal ioctl parameter passing mechanism.
 */

#define _VALIDVICEIOCTL(com) (com >= _VICEIOCTL(0) && com <= _VICEIOCTL(255))

#endif	_SYS_PIOCTL_H_
