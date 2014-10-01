# $NetBSD: Makefile,v 1.7 2012/08/08 13:57:06 christos Exp $

NOMAN=		# defined

.include <bsd.own.mk>

TESTSDIR=		${TESTSBASE}/include/sys

TESTS_C=		t_bitops t_bootblock t_cdefs t_tree t_types

LDADD.t_bitops+=	-lm

.if (${MKRUMP} != "no")
TESTS_C+= t_socket
LDADD.t_socket+=	-lrumpnet_local -lrumpnet_net -lrumpnet
LDADD.t_socket+=	-lrumpvfs -lrump -lrumpuser -lpthread
.endif

.include <bsd.test.mk>
