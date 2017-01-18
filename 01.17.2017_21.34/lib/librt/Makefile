# $NetBSD: Makefile,v 1.2 2011/03/25 09:34:02 jruoho Exp $

NOMAN=		# defined

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/librt

LDADD+=		-lrt

TESTS_C=	t_sched
TESTS_C+=	t_sem

.include <bsd.test.mk>
