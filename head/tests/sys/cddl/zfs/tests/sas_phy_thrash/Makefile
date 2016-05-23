# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/sas_phy_thrash
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	sas_phy_thrash_test

${PACKAGE}FILES+=	sas_phy_thrash_001_pos.ksh
${PACKAGE}FILES+=	sas_phy_thrash.cfg
${PACKAGE}FILES+=	setup.ksh
${PACKAGE}FILES+=	cleanup.ksh

.include <bsd.test.mk>
