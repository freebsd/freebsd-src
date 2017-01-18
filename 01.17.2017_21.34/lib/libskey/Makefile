# $NetBSD: Makefile,v 1.1 2010/07/16 13:56:32 jmmv Exp $

NOMAN=		# defined

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libskey

DPADD+=		${LIBSKEY}
LDADD+=		-lskey

TESTS_C=	t_algorithms

.include <bsd.test.mk>
