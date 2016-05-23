# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/delegate
FILESDIR=${TESTSDIR}

${PACKAGE}FILES+=	delegate.cfg
${PACKAGE}FILES+=	delegate_common.kshlib
${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	setup.ksh

TESTS_SUBDIRS+=	zfs_allow
TESTS_SUBDIRS+=	zfs_unallow

.include <bsd.test.mk>
