# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/scrub_mirror
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	scrub_mirror_test

FILES+=	scrub_mirror_004_pos.ksh
FILES+=	setup.ksh
FILES+=	scrub_mirror_001_pos.ksh
FILES+=	scrub_mirror_003_pos.ksh
FILES+=	cleanup.ksh
FILES+=	default.cfg
FILES+=	scrub_mirror_002_pos.ksh
FILES+=	scrub_mirror_common.kshlib

.include <bsd.test.mk>
