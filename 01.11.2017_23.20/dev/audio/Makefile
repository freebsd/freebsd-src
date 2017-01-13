#	$NetBSD: Makefile,v 1.5 2016/01/23 21:22:48 christos Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/dev/audio
FILESDIR=	${TESTSDIR}

TESTS_SH=	t_pad

BINDIR=		${TESTSDIR}
MKMAN=no

PROGS=		h_pad

CPPFLAGS+=	-D_KERNTYPES
LDADD+=	-lrumpdev_pad -lrumpdev_audio -lrumpdev -lrumpvfs
LDADD+=	-lrump
LDADD+=	-lrumpuser
LDADD+=	-lrump
LDADD+=	-lpthread

FILES=		t_pad_output.bz2.uue

WARNS=	4
NOMAN=

.include <bsd.test.mk>
