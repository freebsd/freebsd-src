# $NetBSD: Makefile,v 1.12 2016/08/08 14:11:08 pgoyette Exp $

NOMAN=		# defined

.include <bsd.own.mk>

TESTSDIR=		${TESTSBASE}/include/sys

TESTS_C+=		t_bitops
TESTS_C+=		t_bootblock
TESTS_C+=		t_cdefs
TESTS_C+=		t_pslist
TESTS_C+=		t_tree
TESTS_C+=		t_types

CPPFLAGS.t_pslist.c+=	-I${NETBSDSRCDIR}/sys
LDADD.t_bitops+=	-lm

.if (${MKRUMP} != "no") && !defined(BSD_MK_COMPAT_FILE)
TESTS_C+= t_socket
CPPFLAGS.t_socket.c+=	-D_KERNTYPES
LDADD.t_socket+=	-lrumpnet_local -lrumpnet_net -lrumpnet
LDADD.t_socket+=	-lrumpvfs -lrump -lrumpuser -lpthread -lrumpdev
.endif

.include <bsd.test.mk>
