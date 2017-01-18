# $NetBSD: Makefile,v 1.2 2011/04/21 18:58:20 martin Exp $

NOMAN=		# defined

.include <bsd.own.mk>

WARNS=	4

TESTSDIR=	${TESTSBASE}/lib/libc/setjmp

TESTS_C+=	t_setjmp
TESTS_C+=	t_threadjmp

LDFLAGS.t_threadjmp+= -pthread

.include <bsd.test.mk>
