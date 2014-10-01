# $NetBSD: Makefile,v 1.2 2014/06/10 04:28:40 he Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net/bpfilter

TESTS_C=	t_bpfilter

LDADD+=		-lrumpnet_shmif
LDADD+=		-lrumpdev_bpf -lrumpdev -lrumpnet_netinet -lrumpnet_net
LDADD+=		-lrumpnet -lrumpvfs -lrump -lrumpuser -lrump -lpthread

.include <bsd.test.mk>
