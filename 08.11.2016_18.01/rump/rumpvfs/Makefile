# $NetBSD: Makefile,v 1.4 2014/06/10 04:28:40 he Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/rump/rumpvfs

TESTS_C=	t_basic
TESTS_C+=	t_etfs
TESTS_C+=	t_p2kifs

LDADD=	-lrumpvfs -lrump -lrumpuser -lrump -lpthread

WARNS=	4

.include <bsd.test.mk>
