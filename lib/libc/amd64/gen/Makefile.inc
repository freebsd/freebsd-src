#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/amd64/gen/Makefile.inc,v 1.21 2003/05/10 00:47:52 peter Exp $

SRCS+=	_setjmp.S setjmp.S sigsetjmp.S \
	fabs.S modf.S \
	frexp.c infinity.c isinf.c ldexp.c
