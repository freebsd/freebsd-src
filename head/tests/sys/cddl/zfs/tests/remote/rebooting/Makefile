# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/remote/rebooting
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	rebooting_test

FILES+=	rebooting.cfg
FILES+=	r_setup.ksh
FILES+=	r_dir_ops.ksh
FILES+=	r_cleanup.ksh
FILES+=	rebooting_001_pos.ksh
FILES+=	cleanup.ksh
FILES+=	setup.ksh
FILES+=	r_verify_booting.ksh

.include <bsd.test.mk>
