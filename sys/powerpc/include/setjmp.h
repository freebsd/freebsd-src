/*-
 *	$NetBSD: setjmp.h,v 1.3 1998/09/16 23:51:27 thorpej Exp $
 * $FreeBSD$
 */

#ifndef _MACHINE_SETJMP_H_
#define	_MACHINE_SETJMP_H_

#define	_JBLEN	100

/*
 * jmp_buf and sigjmp_buf are encapsulated in different structs to force
 * compile-time diagnostics for mismatches.  The structs are the same
 * internally to avoid some run-time errors for mismatches.
 */
#ifndef _ANSI_SOURCE
typedef	struct _sigjmp_buf { long _sjb[_JBLEN + 1]; } sigjmp_buf[1];
#endif

typedef	struct _jmp_buf { long _jb[_JBLEN + 1]; } jmp_buf[1];

#endif /* !_MACHINE_SETJMP_H_ */
