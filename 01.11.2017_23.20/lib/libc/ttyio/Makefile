# $NetBSD: Makefile,v 1.2 2011/01/13 03:19:57 pgoyette Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libc/ttyio

TESTS_C+=	t_ptm
TESTS_C+=	t_ttyio

LDADD.t_ttyio+=	-lutil

.include <bsd.test.mk>
