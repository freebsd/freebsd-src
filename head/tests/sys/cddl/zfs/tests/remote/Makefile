# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/remote
FILESDIR=${TESTSDIR}

FILES+=	remote_common.kshlib
FILES+=	setup.ksh
FILES+=	cleanup.ksh

TESTS_SUBDIRS+=	cross_endian
TESTS_SUBDIRS+=	rebooting
TESTS_SUBDIRS+=	sharing

.include <bsd.test.mk>
