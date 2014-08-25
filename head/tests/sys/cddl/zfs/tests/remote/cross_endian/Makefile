# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/remote/cross_endian
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	cross_endian_test

FILES+=	setup.ksh
FILES+=	cross_endian.cfg
FILES+=	cross_endian_002_pos.ksh
FILES+=	r_verify_recv.ksh
FILES+=	cleanup.ksh
FILES+=	cross_endian_001_pos.ksh
FILES+=	r_verify_import.ksh
FILES+=	r_cleanup.ksh
FILES+=	cross_endian_common.kshlib

.include <bsd.test.mk>
