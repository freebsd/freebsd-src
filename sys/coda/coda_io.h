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
 * 	@(#) src/sys/cfs/cfsio.h,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $ 
 *  $Id: $
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
 * $Log: cfsio.h,v $
 * Revision 1.1.1.1  1998/08/29 21:14:52  rvb
 * Very Preliminary Coda
 *
 * Revision 1.5  1998/08/18 17:05:23  rvb
 * Don't use __RCSID now
 *
 * Revision 1.4  1998/08/18 16:31:47  rvb
 * Sync the code for NetBSD -current; test on 1.3 later
 *
 * Revision 1.3  98/01/23  11:53:49  rvb
 * Bring RVB_CFS1_1 to HEAD
 * 
 * Revision 1.2.38.1  97/12/16  12:40:22  rvb
 * Sync with 1.3
 * 
 * Revision 1.2  96/01/02  16:57:15  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 * 
 * Revision 1.1.2.1  1995/12/20 01:57:42  bnoble
 * Added CFS-specific files
 *
 * Revision 3.1.1.1  1995/03/04  19:08:20  bnoble
 * Branch for NetBSD port revisions
 *
 * Revision 3.1  1995/03/04  19:08:20  bnoble
 * Bump to major revision 3 to prepare for NetBSD port
 *
 * Revision 2.1  1994/07/21  16:25:25  satya
 * Conversion to C++ 3.0; start of Coda Release 2.0
 *
 * Revision 1.3  94/06/14  16:53:47  dcs
 * Added support for ODY-like mounting in the kernel (SETS)
 * 
 * Revision 1.3  94/06/14  16:48:03  dcs
 * Added support for ODY-like mounting in the kernel (SETS)
 * 
 * Revision 1.2  92/10/27  17:58:28  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 1.1  92/04/03  17:35:34  satya
 * Initial revision
 * 
 * Revision 1.5  91/02/09  12:53:26  jjk
 * Substituted rvb's history blurb so that we agree with Mach 2.5 sources.
 * 
 * Revision 2.2.1.1  91/01/06  22:08:22  rvb
 * 	Created for the Coda File System.
 * 	[90/05/23            dcs]
 * 
 * Revision 1.3  90/07/19  10:23:05  dcs
 * Added ; to cfs_resize definition for port to 386.
 * 
 * Revision 1.2  90/05/31  17:02:09  dcs
 * Prepare for merge with facilities kernel.
 * 
 * 
 * 
 */

#ifndef _CFSIO_H_
#define _CFSIO_H_

/* Define ioctl commands for vcioctl, /dev/cfs */

#define CFSRESIZE    _IOW('c', 1, struct cfs_resize )  /* Resize CFS NameCache */
#define CFSSTATS      _IO('c', 2)                      /* Collect stats */
#define CFSPRINT      _IO('c', 3)                      /* Print Cache */
#define CFSTEST       _IO('c', 4)                      /* Print Cache */

struct cfs_resize { int hashsize, heapsize; };

#endif
