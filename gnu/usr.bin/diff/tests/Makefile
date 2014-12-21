# $FreeBSD$

TESTSRC=	${.CURDIR}/../../../../contrib/netbsd-tests/usr.bin/diff
.PATH: ${TESTSRC}

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/gnu/usr.bin/diff
ATF_TESTS_SH=	diff_test
ATF_TESTS_SH_SED_diff_test=	-e 's/t_diff/`basename $$0`/g'
ATF_TESTS_SH_SRC_diff_test=	t_diff.sh

FILESDIR=	${TESTSDIR}
FILES+=		d_mallocv1.in
FILES+=		d_mallocv2.in

.include <bsd.test.mk>
