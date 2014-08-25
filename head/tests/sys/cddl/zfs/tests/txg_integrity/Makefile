# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/txg_integrity
FILESDIR=${TESTSDIR}
BINDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	txg_integrity_test

FILES+=	setup.ksh
FILES+=	txg_integrity_001_pos.ksh
FILES+=	fsync_integrity_001_pos.ksh
FILES+=	cleanup.ksh
FILES+=	txg_integrity.cfg
PROGS+=	txg_integrity
PROGS+=	fsync_integrity
LDADD+=	-lpthread

SRCS.fsync_integrity=	fsync_integrity.c
SRCS.txg_integrity=	txg_integrity.c

.include <bsd.test.mk>
