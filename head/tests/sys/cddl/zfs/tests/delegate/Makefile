# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/delegate
FILESDIR=${TESTSDIR}

FILES+=	delegate.cfg
FILES+=	delegate_common.kshlib
FILES+=	cleanup.ksh
FILES+=	setup.ksh

TESTS_SUBDIRS+=	zfs_allow
TESTS_SUBDIRS+=	zfs_unallow

.include <bsd.test.mk>
