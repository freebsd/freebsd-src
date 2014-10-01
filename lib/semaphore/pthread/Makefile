# $NetBSD: Makefile,v 1.3 2014/06/10 04:28:39 he Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/semaphore/pthread

TESTS_C=	t_sem_pth

CPPFLAGS+=	-I${.CURDIR}/..

LDADD+=		-lrump -lrumpuser -lrump -lpthread

WARNS=	4

.include <bsd.test.mk>
