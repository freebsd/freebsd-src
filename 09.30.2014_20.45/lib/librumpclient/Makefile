#	$NetBSD: Makefile,v 1.5 2012/12/02 17:36:10 pgoyette Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/librumpclient

TESTS_C=	t_fd

TESTS_SH=	t_exec

BINDIR=		${TESTSDIR}
MKMAN=		no

PROGS+=		h_exec
PROGS+=		h_execthr

LDADD+= -lrumpclient
LDADD.h_execthr= -lpthread

.include <bsd.test.mk>
