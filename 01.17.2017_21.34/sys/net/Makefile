# $NetBSD: Makefile,v 1.2 2014/12/03 08:14:26 ozaki-r Exp $
#
WARNS?=6

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/sys/net
CPPFLAGS+=	-I${NETBSDSRCDIR}/sys -D_TEST

TESTS_C=	t_print

.include <bsd.test.mk>
