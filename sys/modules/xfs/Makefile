# $FreeBSD: src/sys/modules/xfs/Makefile,v 1.3 2006/06/09 06:10:17 rodrigc Exp $

.PATH:	${.CURDIR}/../../gnu/fs/xfs \
	${.CURDIR}/../../gnu/fs/xfs/FreeBSD \
	${.CURDIR}/../../gnu/fs/xfs/FreeBSD/support

KMOD=	 xfs

WERROR=

SRCS =  vnode_if.h \
	xfs_alloc.c \
	xfs_alloc_btree.c \
	xfs_bit.c \
	xfs_bmap.c \
	xfs_bmap_btree.c \
	xfs_btree.c \
	xfs_buf_item.c \
	xfs_da_btree.c \
	xfs_dir.c \
	xfs_dir2.c \
	xfs_dir2_block.c \
	xfs_dir2_data.c \
	xfs_dir2_leaf.c \
	xfs_dir2_node.c \
	xfs_dir2_sf.c \
	xfs_dir2_trace.c \
	xfs_dir_leaf.c \
	xfs_error.c \
	xfs_extfree_item.c \
	xfs_freebsd_iget.c \
	xfs_fsops.c \
	xfs_ialloc.c \
	xfs_ialloc_btree.c \
	xfs_inode.c \
	xfs_inode_item.c \
	xfs_iocore.c \
	xfs_itable.c \
	xfs_dfrag.c \
	xfs_log.c \
	xfs_log_recover.c \
	xfs_mount.c \
	xfs_rename.c \
	xfs_trans.c \
	xfs_trans_ail.c \
	xfs_trans_buf.c \
	xfs_trans_extfree.c \
	xfs_trans_inode.c \
	xfs_trans_item.c \
	xfs_utils.c \
	xfs_vfsops.c \
	xfs_vnodeops.c \
	xfs_rw.c \
	xfs_iget.c \
	xfs_attr_leaf.c \
	xfs_attr.c \
	xfsrtstubs.c \
	xfsquotasstubs.c \
	xfsdmapistubs.c \
	xfs_dmops.c \
	xfs_qmops.c \
	xfs_mountops.c \
	xfs_vnops.c \
	xfs_frw.c \
	xfs_iomap.c \
	xfs_buf.c \
	xfs_globals.c \
	xfs_dmistubs.c \
	xfs_behavior.c \
	xfs_super.c \
	xfs_stats.c \
	xfs_sysctl.c \
	xfs_vfs.c \
	xfs_vnode.c \
	xfs_fs_subr.c \
	xfs_ioctl.c \
	debug.c \
	ktrace.c \
	mrlock.c \
	uuid.c \
	kmem.c \
	kdb.c

SRCS+=	opt_ddb.h

.include <bsd.kmod.mk>

CFLAGS+= -I${.CURDIR}/../../gnu/fs/xfs/FreeBSD \
	 -I${.CURDIR}/../../gnu/fs/xfs/FreeBSD/support \
	 -I${.CURDIR}/../../gnu/fs/xfs

#
# XFS sources trigger missing-prototypes warnings.
# Disable them here.
#
CWARNFLAGS+= -Wno-missing-prototypes
