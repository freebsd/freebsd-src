# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/rename_dirs
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	rename_dirs_test

${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	rename_dirs_001_pos.ksh
${PACKAGE}FILES+=	setup.ksh
${PACKAGE}FILES+=	rename_dirs.cfg

.include <bsd.test.mk>
