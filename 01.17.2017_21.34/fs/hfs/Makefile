#	$NetBSD: Makefile,v 1.1 2011/02/18 13:07:54 pooka Exp $
#

TESTSDIR=	${TESTSBASE}/fs/hfs

TESTS_C=	t_pathconvert

FILES=		colon.hfs.bz2.uue
FILESDIR=	${TESTSDIR}

LDADD+=	-lrumpfs_hfs						# fs drivers
LDADD+=	-lrumpdev_disk -lrumpdev				# disk drivers
LDADD+=	-lrumpvfs -lrump -lrumpuser -lpthread			# base

.include <bsd.test.mk>
