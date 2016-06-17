#
# Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it would be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# Further, this software is distributed without any warranty that it is
# free of the rightful claim of any third person regarding infringement
# or the like.  Any license provided herein, whether implied or
# otherwise, applies only to this software file.  Patent licenses, if
# any, provided herein do not apply to combinations of this program with
# other software, or any other product whatsoever.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write the Free Software Foundation, Inc., 59
# Temple Place - Suite 330, Boston MA 02111-1307, USA.
#
# Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
# Mountain View, CA  94043, or:
#
# http://www.sgi.com
#
# For further information regarding this notice, see:
#
# http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
#

EXTRA_CFLAGS +=  -I. -I linux -funsigned-char

ifeq ($(CONFIG_XFS_DEBUG),y)
	EXTRA_CFLAGS += -g -DSTATIC="" -DDEBUG -DXFSDEBUG
	EXTRA_CFLAGS += -DPAGEBUF_LOCK_TRACKING
endif
ifeq ($(CONFIG_XFS_TRACE),y)
	EXTRA_CFLAGS += -DXFS_ALLOC_TRACE
	EXTRA_CFLAGS += -DXFS_ATTR_TRACE
	EXTRA_CFLAGS += -DXFS_BLI_TRACE
	EXTRA_CFLAGS += -DXFS_BMAP_TRACE
	EXTRA_CFLAGS += -DXFS_BMBT_TRACE
	EXTRA_CFLAGS += -DXFS_DIR_TRACE
	EXTRA_CFLAGS += -DXFS_DQUOT_TRACE
	EXTRA_CFLAGS += -DXFS_ILOCK_TRACE
	EXTRA_CFLAGS += -DXFS_LOG_TRACE
	EXTRA_CFLAGS += -DXFS_RW_TRACE
	EXTRA_CFLAGS += -DXFS_DIR2_TRACE
	EXTRA_CFLAGS += -DPAGEBUF_TRACE
	# EXTRA_CFLAGS += -DXFS_VNODE_TRACE
endif

O_TARGET			:= xfs.o
obj-m				:= $(O_TARGET)

obj-$(CONFIG_XFS_RT)		+= xfs_rtalloc.o
obj-$(CONFIG_XFS_POSIX_ACL)	+= xfs_acl.o
obj-$(CONFIG_XFS_POSIX_CAP)	+= xfs_cap.o
obj-$(CONFIG_XFS_POSIX_MAC)	+= xfs_mac.o

obj-$(CONFIG_XFS_TRACE)		+= xfs_dir2_trace.o

obj-y				+= xfs_alloc.o \
				   xfs_alloc_btree.o \
				   xfs_attr.o \
				   xfs_attr_leaf.o \
				   xfs_behavior.o \
				   xfs_bit.o \
				   xfs_bmap.o \
				   xfs_bmap_btree.o \
				   xfs_btree.o \
				   xfs_buf_item.o \
				   xfs_da_btree.o \
				   xfs_dir.o \
				   xfs_dir2.o \
				   xfs_dir2_block.o \
				   xfs_dir2_data.o \
				   xfs_dir2_leaf.o \
				   xfs_dir2_node.o \
				   xfs_dir2_sf.o \
				   xfs_dir_leaf.o \
				   xfs_error.o \
				   xfs_extfree_item.o \
				   xfs_fsops.o \
				   xfs_ialloc.o \
				   xfs_ialloc_btree.o \
				   xfs_iget.o \
				   xfs_inode.o \
				   xfs_inode_item.o \
				   xfs_iocore.o \
				   xfs_iomap.o \
				   xfs_itable.o \
				   xfs_dfrag.o \
				   xfs_log.o \
				   xfs_log_recover.o \
				   xfs_macros.o \
				   xfs_mount.o \
				   xfs_rename.o \
				   xfs_trans.o \
				   xfs_trans_ail.o \
				   xfs_trans_buf.o \
				   xfs_trans_extfree.o \
				   xfs_trans_inode.o \
				   xfs_trans_item.o \
				   xfs_utils.o \
				   xfs_vfsops.o \
				   xfs_vnodeops.o \
				   xfs_refcache.o \
				   xfs_rw.o \
				   xfs_dmops.o \
				   xfs_qmops.o

# Objects not built in this directory
obj-y				+= linux/linux_xfs.o \
				   support/support_xfs.o

subdir-$(CONFIG_XFS_FS)		+= linux support

ifeq ($(CONFIG_XFS_DMAPI),y)
  subdir-$(CONFIG_XFS_FS)	+= dmapi
  obj-y				+= dmapi/xfs_dmapi.o
endif

ifeq ($(CONFIG_XFS_QUOTA),y)
  subdir-$(CONFIG_XFS_FS)	+= quota
  obj-y				+= quota/xfs_quota.o
endif

include $(TOPDIR)/Rules.make

# This is really nasty, but Rules.make was never designed for multi directory
# modules.  Keith Owens.

xfs.o: $(patsubst %,_modsubdir_%,$(subdir-m))
