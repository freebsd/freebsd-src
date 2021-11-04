# $FreeBSD$

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/usr.bin/awk/${.CURDIR:T}
TESTSRC=	${SRCTOP}/contrib/one-true-awk/bugs-fixed

ATF_TESTS_SH=	bug_fix_test

.PATH:		${TESTSRC}
# Upstream controls the list of tests, so generate it dynamically
${PACKAGE}FILES!=(cd ${TESTSRC} ; ls *.awk *.ok *.in *.err)

.include <bsd.test.mk>
