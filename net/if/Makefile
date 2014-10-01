# $NetBSD: Makefile,v 1.2 2014/06/10 04:28:40 he Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net/if

TESTS_C=	t_compat

LDADD+=		-lrumpnet_shmif -lrumpnet_netinet -lrumpnet_net -lrumpnet -lrump
LDADD+=		-lrumpuser -lrump -lpthread

.include <bsd.test.mk>
