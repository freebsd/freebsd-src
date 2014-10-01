#	$NetBSD: Makefile,v 1.2 2014/06/10 04:28:40 he Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net/if_loop

TESTS_C=	t_pr

LDADD+=		-lrumpnet_netinet -lrumpnet_net -lrumpnet
LDADD+=		-lrump -lrumpuser -lrump -lpthread

.include <bsd.test.mk>
