# $NetBSD: Makefile,v 1.4 2016/01/23 21:22:49 christos Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/semaphore/pthread

TESTS_C=	t_sem_pth

CPPFLAGS+=	-I${.CURDIR}/.. -D_KERNTYPES

LDADD+=		-lrump -lrumpuser -lrump -lpthread

WARNS=	4

.include <bsd.test.mk>
