# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/sas_phy_thrash
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	sas_phy_thrash_test

FILES+=	sas_phy_thrash_001_pos.ksh
FILES+=	sas_phy_thrash.cfg
FILES+=	setup.ksh
FILES+=	cleanup.ksh

.include <bsd.test.mk>
