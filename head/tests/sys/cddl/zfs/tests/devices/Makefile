# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/devices
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	devices_test

FILES+=	devices_common.kshlib
FILES+=	devices_002_neg.ksh
FILES+=	devices_test.sh
FILES+=	devices_003_pos.ksh
FILES+=	devices.cfg
FILES+=	devices_001_pos.ksh
FILES+=	cleanup.ksh
FILES+=	setup.ksh

.include <bsd.test.mk>
