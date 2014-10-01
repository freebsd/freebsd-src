# $NetBSD: Makefile,v 1.2 2014/06/10 04:28:39 he Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/kernel/tty

TESTS_C=	t_pr

LDADD+=		-lrumpkern_tty -lrumpvfs -lrump -lrumpuser
LDADD+=		-lrump
LDADD+=		-lpthread

.include <bsd.test.mk>
