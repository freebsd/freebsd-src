#	$NetBSD: Makefile,v 1.6 2016/01/23 21:22:48 christos Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/dev/md
FILESDIR=	${TESTSDIR}

TESTS_SH=	t_md

BINDIR=		${TESTSDIR}
MKMAN=no

PROGS=		h_mdserv

CPPFLAGS+=	-D_KERNTYPES
LDADD+=	-lrumpdev_md -lrumpdev_disk -lrumpdev -lrumpvfs
LDADD+=	-lrumpkern_sysproxy -lrump
LDADD+=	-lrumpuser
LDADD+=	-lpthread

WARNS=	4
NOMAN=

.include <bsd.test.mk>
