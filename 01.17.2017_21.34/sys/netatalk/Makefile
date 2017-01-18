# $NetBSD: Makefile,v 1.1 2014/12/02 19:48:21 christos Exp $
#
WARNS?=6

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/sys/netatalk
CPPFLAGS+=	-I${NETBSDSRCDIR}/sys -D_TEST

TESTS_C=	t_print

.include <bsd.test.mk>
