# $NetBSD: Makefile,v 1.1 2012/03/17 16:33:14 jruoho Exp $

NOMAN=		# defined

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/usr.bin/m4

TESTS_SH=	t_m4

FILESDIR=	${TESTSDIR}
FILES=		d_ff_after_dnl.m4.uue
FILES+=		d_ff_after_dnl.out
FILES+=		d_m4wrap-P.m4
FILES+=		d_m4wrap-P.out
FILES+=		d_m4wrap.m4
FILES+=		d_m4wrap.out

.include <bsd.test.mk>
