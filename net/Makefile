# $NetBSD: Makefile,v 1.28 2016/04/15 01:38:17 ozaki-r Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net

TESTS_SUBDIRS=		fdpass in_cksum net sys
.if (${MKRUMP} != "no") && !defined(BSD_MK_COMPAT_FILE)
TESTS_SUBDIRS+=		arp bpf bpfilter carp icmp if if_bridge if_gif if_loop
TESTS_SUBDIRS+=		if_pppoe if_tap mcast mpls ndp npf route
.if (${MKSLJIT} != "no")
TESTS_SUBDIRS+=		bpfjit
.endif
.endif

.include <bsd.test.mk>
