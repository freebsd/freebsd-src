# $NetBSD: Makefile,v 1.1 2011/12/27 00:47:23 christos Exp $

NOMAN=		# defined 

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libcrypt

LDADD+=		-lcrypt
DPADD+=		${LIBCRYPT}
WARNS?=		4

TESTS_C=	t_crypt

.include <bsd.test.mk>
