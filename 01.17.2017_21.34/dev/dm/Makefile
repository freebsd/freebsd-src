
#	$NetBSD: Makefile,v 1.3 2016/01/23 21:22:48 christos Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/dev/dm
FILESDIR=	${TESTSDIR}

TESTS_SH=	t_dm

BINDIR=		${TESTSDIR}
MKMAN=no

PROGS=		h_dm

CPPFLAGS+=-D_KERNTYPES
LDADD+= -lprop -lutil
LDADD+= -lrumpdev_disk -lrumpdev_dm
LDADD+=	-lrumpdev -lrumpvfs
LDADD+=	-lrump
LDADD+=	-lrumpuser -lpthread

FILES=		t_dm_disk_1.bz2.uue t_dm_disk_2.bz2.uue t_dm_disk_3.bz2.uue

WARNS=	4
NOMAN=

.include <bsd.test.mk>
