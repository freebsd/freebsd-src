# $FreeBSD$

.include <src.opts.mk>

PACKAGE=	tests
TESTSDIR=	${TESTSBASE}/sys/cddl/zfs/tests/rootpool
FILESDIR=	${TESTSDIR}

ATF_TESTS_KSH93+=	rootpool_test

${PACKAGE}FILES+=	rootpool_006_pos.ksh
${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	rootpool_003_neg.ksh
${PACKAGE}FILES+=	rootpool_007_neg.ksh
${PACKAGE}FILES+=	rootpool_002_neg.ksh
${PACKAGE}FILES+=	setup.ksh
${PACKAGE}FILES+=	rootpool_005_pos.ksh
${PACKAGE}FILES+=	rootpool_001_pos.ksh
${PACKAGE}FILES+=	rootpool_004_pos.ksh

.include <bsd.test.mk>
