# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/threadsappend
FILESDIR=${TESTSDIR}
BINDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	threadsappend_test

${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	threadsappend_001_pos.ksh
${PACKAGE}FILES+=	threadsappend.cfg
${PACKAGE}FILES+=	setup.ksh
PROG+=	threadsappend
LIBADD+=	pthread
MAN=

.include <bsd.test.mk>
