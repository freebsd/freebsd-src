/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
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
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	from: Mach, Revision 2.7  92/02/29  15:33:41  rpd
 *	$Id: real_prot.h,v 1.6 1997/02/22 09:29:54 peter Exp $
 */

/*
 * Copyright (C) 1994 by HOSOKAWA, Tatsumi <hosokawa@jp.FreeBSD.org>
 *
 * This software may be used, modified, copied, and distributed, in
 * both source and binary form provided that the above copyright and
 * these terms are retained. Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its
 * use.
 *
 * Sep., 1994	Implemented on FreeBSD 1.1.5.1R (Toshiba AVS001WD)
 * Nov., 1994	Committed to FreeBSD 2.0-current
 * Jan., 1995	Ported to RT-Mach 3.0 MK83g
 */

/*
 * Modified to APM BIOS initializer by HOSOKAWA, Tatsumi
 */

#define ALIGN	4
#define EXT(x) _ ## x
#define LEXT(x) _ ## x ## :

#define addr32	.byte 0x67
#define data32	.byte 0x66

#define	ENTRY(x)	.globl EXT(x); .align ALIGN; LEXT(x)
