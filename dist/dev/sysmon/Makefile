#	$NetBSD: Makefile,v 1.3 2014/06/10 04:28:39 he Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/dev/sysmon

TESTS_C=	t_swwdog

LDADD+=	-lrumpdev_sysmon -lrumpdev -lrumpvfs
LDADD+=	-lrump
LDADD+=	-lrumpuser
LDADD+=	-lrump
LDADD+=	-lpthread

WARNS=	4

TESTS_SH=	t_swsensor

.include <bsd.test.mk>
