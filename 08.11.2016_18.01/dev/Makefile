#	$NetBSD: Makefile,v 1.11 2016/07/29 06:13:39 pgoyette Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/dev

TESTS_SUBDIRS+=	cgd fss raidframe
.if (${MKRUMP} != "no") && !defined(BSD_MK_COMPAT_FILE) 
TESTS_SUBDIRS+=	audio md scsipi sysmon usb
.endif


.include <bsd.test.mk>
