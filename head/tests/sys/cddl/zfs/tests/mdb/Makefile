# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/mdb
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	mdb_test

${PACKAGE}FILES+=	mdb.cfg
${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	setup.ksh
${PACKAGE}FILES+=	mdb_001_pos.ksh

.include <bsd.test.mk>
