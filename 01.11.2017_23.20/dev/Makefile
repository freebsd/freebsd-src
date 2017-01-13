#	$NetBSD: Makefile,v 1.12 2016/08/14 14:55:42 jakllsch Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/dev

TESTS_SUBDIRS+=	cgd clock_subr fss raidframe
.if (${MKRUMP} != "no") && !defined(BSD_MK_COMPAT_FILE) 
TESTS_SUBDIRS+=	audio md scsipi sysmon usb
.endif


.include <bsd.test.mk>
