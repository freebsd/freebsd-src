# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/compression
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	compression_test

${PACKAGE}FILES+=	compress_004_pos.ksh
${PACKAGE}FILES+=	compress_001_pos.ksh
${PACKAGE}FILES+=	compress_003_pos.ksh
${PACKAGE}FILES+=	setup.ksh
${PACKAGE}FILES+=	compress.cfg
${PACKAGE}FILES+=	cleanup.ksh

.include <bsd.test.mk>
