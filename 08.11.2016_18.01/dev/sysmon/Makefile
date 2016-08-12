#	$NetBSD: Makefile,v 1.4 2016/01/23 21:22:48 christos Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/dev/sysmon

TESTS_C=	t_swwdog

CPPFLAGS+=	-D_KERNTYPES
LDADD+=	-lrumpdev_sysmon -lrumpdev -lrumpvfs
LDADD+=	-lrump
LDADD+=	-lrumpuser
LDADD+=	-lrump
LDADD+=	-lpthread

WARNS=	4

TESTS_SH=	t_swsensor

.include <bsd.test.mk>
