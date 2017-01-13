#	$NetBSD: Makefile,v 1.4 2015/06/22 00:05:23 matt Exp $
#
.include <bsd.own.mk>

.if (${MKRUMP} != "no") && !defined(BSD_MK_COMPAT_FILE)
TESTS_SUBDIRS=		pthread
.endif

TESTSDIR=	${TESTSBASE}/lib/semaphore

.include <bsd.test.mk>
