/*
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software_Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 **********************************************************************
 * HISTORY
 * $Log: supcvers.c,v $
 * Revision 1.1.1.1  1993/08/21  00:46:34  jkh
 * Current sup with compression support.
 *
 * Revision 1.1.1.1  1993/05/21  14:52:19  cgd
 * initial import of CMU's SUP to NetBSD
 *
 * Revision 1.8  92/08/11  12:07:51  mrt
 * 	Added use-rel-suffix option. Picked up STUMP changes.
 * 	[V7.26]
 * 
 * Revision 1.7  92/02/08  18:24:23  mja
 * 	Support for -k/-K and "keep" option. [V7.25]
 * 	[92/01/17            vdelvecc]
 * 
 * 27-Dec-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added crosspatch support.  Removed nameserver support. [V7.24]
 *
 * 28-Jun-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added code for "release" support. [V6.23]
 *
 * 26-May-87  Doug Philips (dwp) at Carnegie-Mellon University
 *	Changes for Version 6, better supoort to reflect errors to
 *	logfile. [V6.22]
 *
 * 25-May-87  Doug Philips (dwp) at Carnegie-Mellon University
 *	Split sup.c into subparts. [V5.21]
 *
 * 20-May-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Changes to many files to make lint happy. [V5.20]
 *
 * 01-Apr-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Changes to sup.c and scmio.c. [V5.19]
 *
 * 19-Sep-86  Mike Accetta (mja) at Carnegie-Mellon University
 *	Changes to sup.c. [V5.18]
 *
 * 21-Jun-86  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Minor bug fix to previous edit in sup.c. [V5.17]
 *
 * 07-Jun-86  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Changes to sup.c and sup.h. [V5.16]
 *
 * 30-May-86  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added changes to sup.c, sup.h, scm.c, scmio.h. [V5.15]
 *
 * 19-Feb-86  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Created. [V5.14]
 *
 **********************************************************************
 */

int PGMVERSION = 26;		/* program version of sup */
