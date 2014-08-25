# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/compression
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	compression_test

FILES+=	compress_004_pos.ksh
FILES+=	compress_001_pos.ksh
FILES+=	compress_003_pos.ksh
FILES+=	setup.ksh
FILES+=	compress.cfg
FILES+=	cleanup.ksh
FILES+=	compress_002_pos.ksh

.include <bsd.test.mk>
