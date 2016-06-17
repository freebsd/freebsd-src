#
# Makefile for the Linux JFS filesystem routines.
#

O_TARGET := jfs.o
obj-y   := super.o file.o inode.o namei.o jfs_mount.o jfs_umount.o \
	    jfs_xtree.o jfs_imap.o jfs_debug.o jfs_dmap.o \
	    jfs_unicode.o jfs_dtree.o jfs_inode.o \
	    jfs_extent.o symlink.o jfs_metapage.o \
	    jfs_logmgr.o jfs_txnmgr.o jfs_uniupr.o resize.o xattr.o
obj-m   := $(O_TARGET)

EXTRA_CFLAGS += -D_JFS_4K

include $(TOPDIR)/Rules.make
