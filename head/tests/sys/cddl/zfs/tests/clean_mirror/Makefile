# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/clean_mirror
FILESDIR=${TESTSDIR}

${PACKAGE}FILES+=	setup.ksh
${PACKAGE}FILES+=	default.cfg
${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	clean_mirror_common.kshlib

TESTS_SUBDIRS+=	assertion_001
TESTS_SUBDIRS+=	assertion_002
TESTS_SUBDIRS+=	assertion_003
TESTS_SUBDIRS+=	assertion_004

.include <bsd.test.mk>
