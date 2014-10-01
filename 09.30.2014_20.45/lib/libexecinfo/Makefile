# $NetBSD: Makefile,v 1.5 2014/03/18 18:20:44 riastradh Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libexecinfo

TESTS_C+=	t_backtrace

LDADD+=		-lexecinfo -lelf
DPADD+=		${LIBEXECINFO} ${LIBELF}

COPTS.t_backtrace.c=	-Wno-stack-protector

.include <bsd.test.mk>
