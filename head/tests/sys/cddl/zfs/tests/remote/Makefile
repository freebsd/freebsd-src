# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/remote
FILESDIR=${TESTSDIR}

${PACKAGE}FILES+=	remote_common.kshlib
${PACKAGE}FILES+=	setup.ksh
${PACKAGE}FILES+=	cleanup.ksh

TESTS_SUBDIRS+=	cross_endian
TESTS_SUBDIRS+=	rebooting
TESTS_SUBDIRS+=	sharing

.include <bsd.test.mk>
