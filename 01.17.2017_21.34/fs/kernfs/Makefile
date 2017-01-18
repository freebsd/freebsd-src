#	$NetBSD: Makefile,v 1.4 2014/06/10 04:28:39 he Exp $
#

TESTSDIR=	${TESTSBASE}/fs/kernfs
WARNS=		4

TESTS_C=	t_basic

.include <bsd.own.mk>

LDADD+=	-lrumpfs_kernfs						# fs driver
LDADD+=	-lrumpvfs -lrump -lrumpuser -lrump -lpthread		# base

.include <bsd.test.mk>
