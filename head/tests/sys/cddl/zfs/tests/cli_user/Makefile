# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_user
FILESDIR=${TESTSDIR}

FILES+=	setup.ksh
FILES+=	cleanup.ksh
FILES+=	cli.cfg

TESTS_SUBDIRS+=	zpool_iostat
TESTS_SUBDIRS+=	zpool_list
TESTS_SUBDIRS+=	zfs_list
TESTS_SUBDIRS+=	misc

.include <bsd.test.mk>
