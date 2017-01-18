# $NetBSD: Makefile,v 1.2 2012/03/11 18:35:59 jruoho Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/usr.bin/awk
TESTS_SH=	t_awk
NOMAN=		# defined

FILESDIR=	${TESTSDIR}
FILES=		d_big_regexp.awk
FILES+=		d_big_regexp.in
FILES+=		d_big_regexp.out
FILES+=		d_end1.awk
FILES+=		d_end1.in
FILES+=		d_end1.out
FILES+=		d_end2.awk
FILES+=		d_end2.in
FILES+=		d_end2.out
FILES+=		d_period.awk
FILES+=		d_period.in
FILES+=		d_period.out
FILES+=		d_string1.awk
FILES+=		d_string1.out
FILES+=		d_tolower.awk
FILES+=		d_tolower.in
FILES+=		d_tolower.out
FILES+=		d_toupper.awk
FILES+=		d_toupper.in
FILES+=		d_toupper.out
FILES+=		d_assign_NF.awk
FILES+=		d_assign_NF.in
FILES+=		d_assign_NF.out

.include <bsd.test.mk>