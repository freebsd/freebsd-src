#	$NetBSD: Makefile,v 1.4 2014/06/09 18:22:05 he Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/dev/md
FILESDIR=	${TESTSDIR}

TESTS_SH=	t_md

BINDIR=		${TESTSDIR}
MKMAN=no

PROGS=		h_mdserv

LDADD+=	-lrumpdev_md -lrumpdev_disk -lrumpdev -lrumpvfs
LDADD+=	-lrump
LDADD+=	-lrumpuser
LDADD+=	-lrump
LDADD+=	-lpthread

WARNS=	4
NOMAN=

.include <bsd.test.mk>
