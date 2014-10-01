#	$NetBSD: Makefile,v 1.8 2012/08/08 13:57:05 christos Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/dev

TESTS_SUBDIRS+=	cgd raidframe
.if (${MKRUMP} != "no")
TESTS_SUBDIRS+=	audio md scsipi sysmon
.endif


.include <bsd.test.mk>
