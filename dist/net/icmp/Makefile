# $NetBSD: Makefile,v 1.4 2014/06/10 04:28:40 he Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net/icmp

TESTS_C=	t_forward
TESTS_C+=	t_ping

TESTS_SH=	t_ping2

LDADD+=		-lrumpnet_shmif -lrumpnet_netinet -lrumpnet_net -lrumpnet
LDADD+=		-lrump -lrumpuser -lrump -lpthread

.include <bsd.test.mk>
