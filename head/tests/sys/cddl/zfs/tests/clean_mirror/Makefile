# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/clean_mirror
FILESDIR=${TESTSDIR}

FILES+=	setup.ksh
FILES+=	default.cfg
FILES+=	cleanup.ksh
FILES+=	clean_mirror_common.kshlib

TESTS_SUBDIRS+=	assertion_001
TESTS_SUBDIRS+=	assertion_002
TESTS_SUBDIRS+=	assertion_003
TESTS_SUBDIRS+=	assertion_004

.include <bsd.test.mk>
