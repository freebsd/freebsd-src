# $FreeBSD$

TESTSRC=	${.CURDIR}/../../../contrib/netbsd-tests/usr.bin/cut
.PATH: ${TESTSRC}

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/usr.bin/cut
ATF_TESTS_SH=	cut_test
ATF_TESTS_SH_SRC_cut_test=	t_cut.sh

FILESDIR=	${TESTSDIR}
FILES=		d_basic.out
FILES+=		d_cut.in
FILES+=		d_dflag.out
FILES+=		d_dsflag.out
FILES+=		d_latin1.in
FILES+=		d_sflag.out
FILES+=		d_utf8.in

.include <bsd.test.mk>
