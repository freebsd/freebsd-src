#	$NetBSD: Makefile,v 1.2 2014/06/10 04:28:40 he Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net/carp

TESTS_C=	t_basic

LDADD+=		-lrumpnet_shmif -lrumpnet_netinet -lrumpnet_net
LDADD+=		-lrumpnet -lrump -lrumpuser -lrump -lpthread

.include <bsd.test.mk>
