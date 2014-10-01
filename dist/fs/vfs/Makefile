#	$NetBSD: Makefile,v 1.19 2013/06/28 13:04:06 reinoud Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/fs/vfs
WARNS=		4

TESTS_C+=	t_full
TESTS_C+=	t_io
TESTS_C+=	t_renamerace
TESTS_C+=	t_ro
TESTS_C+=	t_rmdirrace
TESTS_C+=	t_union
TESTS_C+=	t_unpriv
TESTS_C+=	t_vfsops
TESTS_C+=	t_vnops

LDADD+=-lrumpfs_ext2fs						# ext2fs
LDADD+=-lrumpfs_ffs						# ffs
LDADD+=-lrumpfs_lfs						# lfs
LDADD+=-lrumpfs_msdos						# msdos
LDADD+=-lrumpfs_syspuffs					# puffs
LDADD+=-lrumpdev_putter -lrumpdev				#   \ putter
LDADD+=-lrumpfs_sysvbfs						# sysvbfs
LDADD+=-lrumpfs_tmpfs						# tmpfs
LDADD+=-lrumpfs_udf						# udf
LDADD+=-lrumpfs_union						# union
LDADD+=-lrumpfs_v7fs						# v7fs
LDADD+=-lrumpdev_disk -lrumpdev					# disk device


VFSTESTDIR != cd ${.CURDIR}/../common && ${PRINTOBJDIR}
LDADD+=-L${VFSTESTDIR} -lvfstest

LDADD+=-lrumpfs_nfs						# NFS
LDADD+=-lrumpnet_shmif -lrumpnet_netinet -lrumpnet_net -lrumpnet
LDADD+=-lrumpvfs -lrump -lrumpuser -lpthread			# base

LDADD+=-lutil

.if (${MKZFS} != "no")
LDADD+=-lrumpfs_zfs -lrumpkern_solaris -lrumpdev_rnd
CPPFLAGS+=-DWANT_ZFS_TESTS
.endif

.include <bsd.test.mk>
