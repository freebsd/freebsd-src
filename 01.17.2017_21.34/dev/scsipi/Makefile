#	$NetBSD: Makefile,v 1.7 2016/01/23 21:22:48 christos Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/dev/scsipi

TESTS_C=	t_cd

CPPFLAGS+=	-I${.CURDIR}/libscsitest -D_KERNTYPES

# kernel component required by test
SUBDIR=	libscsitest
SCSITESTDIR!= cd ${.CURDIR}/libscsitest && ${PRINTOBJDIR}
LDFLAGS+= -L${SCSITESTDIR}
LDADD+=	-Wl,--whole-archive -lrumpdev_scsitest -Wl,--no-whole-archive

LDADD+= -lrumpdev_scsipi -lrumpdev_disk -lrumpdev -lrumpvfs
LDADD+=	-lrump -lutil
LDADD+=	-lrumpuser -lpthread
LDADD+= -lrump

WARNS=	4

.include <bsd.test.mk>
