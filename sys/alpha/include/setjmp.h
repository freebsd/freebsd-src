/* $FreeBSD: src/sys/alpha/include/setjmp.h,v 1.3 1999/08/28 00:38:51 peter Exp $ */
/* From: NetBSD: setjmp.h,v 1.2 1997/04/06 08:47:41 cgd Exp */

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * machine/setjmp.h: machine dependent setjmp-related information.
 */

#define _JBLEN  81              /* size, in longs, of a jmp_buf */

/*
 * jmp_buf and sigjmp_buf are encapsulated in different structs to force
 * compile-time diagnostics for mismatches.  The structs are the same
 * internally to avoid some run-time errors for mismatches.
 */
#ifndef _ANSI_SOURCE
typedef struct { long _sjb[_JBLEN + 1]; } sigjmp_buf[1];
#endif /* not ANSI */

typedef struct { long _jb[_JBLEN + 1]; } jmp_buf[1];
