#	$NetBSD: Makefile,v 1.5 2011/01/24 17:51:29 pooka Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/rump/rumpkern/h_client

TESTS_C+=	h_forkcli
TESTS_C+=	h_reconcli
TESTS_C+=	h_sigcli
TESTS_C+=	h_simplecli
TESTS_C+=	h_stresscli

ATFFILE=	no

LDADD+=			-lrumpclient
LDADD.h_stresscli=	-lpthread
LDADD.h_reconcli=	-lpthread

WARNS=	4
NOMAN=

.include <bsd.test.mk>
