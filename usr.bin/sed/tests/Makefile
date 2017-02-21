# $FreeBSD$

NETBSD_ATF_TESTS_SH+=	sed_test
TAP_TESTS_SH=	legacy_test
TAP_TESTS_SH+=	multi_test
TEST_METADATA.multi_test+=	required_files="/usr/share/dict/words"
TAP_TESTS_SH+=	inplace_race_test

FILESDIR=	${TESTSDIR}
FILES=		hanoi.sed
FILES+=		math.sed
FILES+=		regress.G.out
FILES+=		regress.P.out
FILES+=		regress.b2a.out
FILES+=		regress.bcb.out
FILES+=		regress.c0.out
FILES+=		regress.c1.out
FILES+=		regress.c2.out
FILES+=		regress.c3.out
FILES+=		regress.hanoi.out
FILES+=		regress.icase1.out
FILES+=		regress.icase2.out
FILES+=		regress.icase3.out
FILES+=		regress.icase4.out
FILES+=		regress.in
FILES+=		regress.math.out
FILES+=		regress.not.out
FILES+=		regress.psl.out
FILES+=		regress.s3.out
FILES+=		regress.s4.out
FILES+=		regress.s5.out
FILES+=		regress.sg.out
FILES+=		regress.sh
FILES+=		regress.y.out
ATF_TESTS_SH_SED_sed_test+=	-e 's,atf_expect_fail "PR bin/28126",,g'
FILES+=		d_c2048.in

SUBDIR=		regress.multitest.out

.include <netbsd-tests.test.mk>
.include <bsd.test.mk>
