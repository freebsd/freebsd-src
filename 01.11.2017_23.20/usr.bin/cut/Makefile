# $NetBSD: Makefile,v 1.1 2012/03/17 16:33:13 jruoho Exp $

NOMAN=		# defined

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/usr.bin/cut

TESTS_SH=	t_cut

FILESDIR=	${TESTSDIR}
FILES=		d_basic.out
FILES+=		d_cut.in
FILES+=		d_dflag.out
FILES+=		d_dsflag.out
FILES+=		d_latin1.in
FILES+=		d_sflag.out
FILES+=		d_utf8.in

.include <bsd.test.mk>
