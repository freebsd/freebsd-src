#	$NetBSD: Makefile,v 1.7 2016/08/10 21:10:18 kre Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/librumpclient

TESTS_C=	t_fd

TESTS_SH=	t_exec

BINDIR=		${TESTSDIR}
MKMAN=		no

PROGS+=		h_exec
PROGS+=		h_execthr
CPPFLAGS+=	-D_KERNTYPES

LDADD+= -lrumpclient
LDADD+= -lrumpdev
LDADD+= -lrumpvfs
LDADD.h_execthr= -lpthread

.include <bsd.test.mk>
