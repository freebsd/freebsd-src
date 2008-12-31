/*-
 *	$NetBSD: setjmp.h,v 1.3 1998/09/16 23:51:27 thorpej Exp $
 * $FreeBSD: src/sys/powerpc/include/setjmp.h,v 1.4.24.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _MACHINE_SETJMP_H_
#define	_MACHINE_SETJMP_H_

#include <sys/cdefs.h>

#define	_JBLEN	100

/*
 * jmp_buf and sigjmp_buf are encapsulated in different structs to force
 * compile-time diagnostics for mismatches.  The structs are the same
 * internally to avoid some run-time errors for mismatches.
 */
#if __BSD_VISIBLE || __POSIX_VISIBLE || __XSI_VISIBLE
typedef	struct _sigjmp_buf { long _sjb[_JBLEN + 1]; } sigjmp_buf[1];
#endif

typedef	struct _jmp_buf { long _jb[_JBLEN + 1]; } jmp_buf[1];

#endif /* !_MACHINE_SETJMP_H_ */
