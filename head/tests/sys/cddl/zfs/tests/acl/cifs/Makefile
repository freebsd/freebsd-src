# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/acl/cifs
FILESDIR=${TESTSDIR}

${PACKAGE}FILES+=	cifs.kshlib
${PACKAGE}FILES+=	cifs_attr_001_pos.ksh
${PACKAGE}FILES+=	cifs_attr_002_pos.ksh
${PACKAGE}FILES+=	cifs_attr_003_pos.ksh

ATF_TESTS_KSH93+=	cifs_test

.include <bsd.test.mk>
