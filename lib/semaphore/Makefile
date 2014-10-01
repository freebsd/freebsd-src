#	$NetBSD: Makefile,v 1.3 2012/08/08 13:57:06 christos Exp $
#
.include <bsd.own.mk>

.if (${MKRUMP} != "no")
TESTS_SUBDIRS=		pthread
.endif

TESTSDIR=	${TESTSBASE}/lib/semaphore

.include <bsd.test.mk>
