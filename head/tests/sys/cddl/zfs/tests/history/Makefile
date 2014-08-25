# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/history/
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	history_test

FILES+=	history_005_neg.ksh
FILES+=	history_004_pos.ksh
FILES+=	history_008_pos.ksh
FILES+=	i386.orig_history.txt
FILES+=	sparc.orig_history.txt
FILES+=	history_common.kshlib
FILES+=	history.cfg
FILES+=	sparc.migratedpool.DAT.Z
FILES+=	history_010_pos.ksh
FILES+=	setup.ksh
FILES+=	zfs-pool-v4.dat.Z
FILES+=	history_009_pos.ksh
FILES+=	history_001_pos.ksh
FILES+=	history_006_neg.ksh
FILES+=	i386.migratedpool.DAT.Z
FILES+=	history_003_pos.ksh
FILES+=	history_007_pos.ksh
FILES+=	history_002_pos.ksh
FILES+=	cleanup.ksh

.include <bsd.test.mk>
