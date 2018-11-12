# $FreeBSD$

PACKAGE=	tests

TAP_TESTS_SH=	legacy_test
TAP_TESTS_SH+=	multi_test
TEST_METADATA.multi_test+=	required_files="/usr/share/dict/words"
TAP_TESTS_SH+=	inplace_race_test

${PACKAGE}FILES+=		hanoi.sed
${PACKAGE}FILES+=		math.sed
${PACKAGE}FILES+=		regress.G.out
${PACKAGE}FILES+=		regress.P.out
${PACKAGE}FILES+=		regress.b2a.out
${PACKAGE}FILES+=		regress.bcb.out
${PACKAGE}FILES+=		regress.c0.out
${PACKAGE}FILES+=		regress.c1.out
${PACKAGE}FILES+=		regress.c2.out
${PACKAGE}FILES+=		regress.c3.out
${PACKAGE}FILES+=		regress.hanoi.out
${PACKAGE}FILES+=		regress.icase1.out
${PACKAGE}FILES+=		regress.icase2.out
${PACKAGE}FILES+=		regress.icase3.out
${PACKAGE}FILES+=		regress.icase4.out
${PACKAGE}FILES+=		regress.in
${PACKAGE}FILES+=		regress.math.out
${PACKAGE}FILES+=		regress.not.out
${PACKAGE}FILES+=		regress.psl.out
${PACKAGE}FILES+=		regress.s3.out
${PACKAGE}FILES+=		regress.s4.out
${PACKAGE}FILES+=		regress.s5.out
${PACKAGE}FILES+=		regress.sg.out
${PACKAGE}FILES+=		regress.sh
${PACKAGE}FILES+=		regress.y.out

SUBDIR=		regress.multitest.out

.include <bsd.test.mk>
