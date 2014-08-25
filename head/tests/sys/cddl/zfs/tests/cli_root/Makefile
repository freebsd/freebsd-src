# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root
FILESDIR=${TESTSDIR}

FILES+=	cli_common.kshlib
FILES+=	setup.ksh
FILES+=	cli.cfg
FILES+=	cleanup.ksh

TESTS_SUBDIRS+=	zfs_upgrade
TESTS_SUBDIRS+=	zfs_promote
TESTS_SUBDIRS+=	zfs_clone
TESTS_SUBDIRS+=	zfs_property
TESTS_SUBDIRS+=	zfs_destroy
TESTS_SUBDIRS+=	zpool_create
TESTS_SUBDIRS+=	zpool_history
TESTS_SUBDIRS+=	zpool_expand
TESTS_SUBDIRS+=	zpool_remove
TESTS_SUBDIRS+=	zfs_mount
TESTS_SUBDIRS+=	zfs_unshare
TESTS_SUBDIRS+=	zdb
TESTS_SUBDIRS+=	zpool_online
TESTS_SUBDIRS+=	zpool_get
TESTS_SUBDIRS+=	zpool_export
TESTS_SUBDIRS+=	zfs_copies
TESTS_SUBDIRS+=	zfs_get
TESTS_SUBDIRS+=	zfs
TESTS_SUBDIRS+=	zpool_clear
TESTS_SUBDIRS+=	zpool_import
TESTS_SUBDIRS+=	zpool
TESTS_SUBDIRS+=	zpool_offline
TESTS_SUBDIRS+=	zpool_replace
TESTS_SUBDIRS+=	zfs_rollback
TESTS_SUBDIRS+=	zpool_set
TESTS_SUBDIRS+=	zfs_send
TESTS_SUBDIRS+=	zfs_set
TESTS_SUBDIRS+=	zpool_detach
TESTS_SUBDIRS+=	zpool_scrub
TESTS_SUBDIRS+=	zfs_inherit
TESTS_SUBDIRS+=	zfs_snapshot
TESTS_SUBDIRS+=	zfs_share
TESTS_SUBDIRS+=	zpool_destroy
TESTS_SUBDIRS+=	zpool_status
TESTS_SUBDIRS+=	zfs_unmount
TESTS_SUBDIRS+=	zfs_receive
TESTS_SUBDIRS+=	zfs_create
TESTS_SUBDIRS+=	zpool_upgrade
TESTS_SUBDIRS+=	zpool_add
TESTS_SUBDIRS+=	zfs_rename
TESTS_SUBDIRS+=	zpool_attach
TESTS_SUBDIRS+=	zfs_reservation

.include <bsd.test.mk>
