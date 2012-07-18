/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Extensions to the stdio package
 */

#ifndef _STDIO_EXT_H
#define	_STDIO_EXT_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <stdio.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Even though the contents of the stdio FILE structure have always been
 * private to the stdio implementation, over the years some programs have
 * needed to get information about a stdio stream that was not accessible
 * through a supported interface. These programs have resorted to accessing
 * fields of the FILE structure directly, rendering them possibly non-portable
 * to new implementations of stdio, or more likely, preventing enhancements
 * to stdio because those programs will break.
 *
 * In the 64-bit world, the FILE structure is opaque. The routines here
 * are provided as a way to get the information that used to be retrieved
 * directly from the FILE structure. They are based on the needs of
 * existing programs (such as 'mh' and 'emacs'), so they may be extended
 * as other programs are ported. Though they may still be non-portable to
 * other operating systems, they will work from each Solaris release to
 * the next. More portable interfaces are being developed.
 */

#define	FSETLOCKING_QUERY	0
#define	FSETLOCKING_INTERNAL	1
#define	FSETLOCKING_BYCALLER	2

extern size_t __fbufsize(FILE *stream);
extern int __freading(FILE *stream);
extern int __fwriting(FILE *stream);
extern int __freadable(FILE *stream);
extern int __fwritable(FILE *stream);
extern int __flbf(FILE *stream);
extern void __fpurge(FILE *stream);
extern size_t __fpending(FILE *stream);
extern void _flushlbf(void);
extern int __fsetlocking(FILE *stream, int type);

/*
 * Extended FILE enabling function.
 */
#if defined(_LP64) && !defined(__lint)
#define	enable_extended_FILE_stdio(fd, act)		(0)
#else
extern int enable_extended_FILE_stdio(int, int);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _STDIO_EXT_H */
