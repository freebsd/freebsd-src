#
# Makefile for the Linux filesystems.
#
# 14 Sep 2000, Christoph Hellwig <hch@infradead.org>
# Rewritten to use lists instead of if-statements.
# 

O_TARGET := fs.o

export-objs :=	filesystems.o open.o dcache.o buffer.o dquot.o
mod-subdirs :=	nls

obj-y :=	open.o read_write.o devices.o file_table.o buffer.o \
		super.o block_dev.o char_dev.o stat.o exec.o pipe.o namei.o \
		fcntl.o ioctl.o readdir.o select.o fifo.o locks.o \
		dcache.o inode.o attr.o bad_inode.o file.o iobuf.o dnotify.o \
		filesystems.o namespace.o seq_file.o xattr.o quota.o

obj-$(CONFIG_QUOTA)		+= dquot.o quota_v1.o
obj-$(CONFIG_QFMT_V2)		+= quota_v2.o

subdir-$(CONFIG_PROC_FS)	+= proc
subdir-y			+= partitions

# Do not add any filesystems before this line
subdir-$(CONFIG_EXT3_FS)	+= ext3    # Before ext2 so root fs can be ext3
subdir-$(CONFIG_JBD)		+= jbd
subdir-$(CONFIG_EXT2_FS)	+= ext2
subdir-$(CONFIG_CRAMFS)		+= cramfs
subdir-$(CONFIG_RAMFS)		+= ramfs
subdir-$(CONFIG_CODA_FS)	+= coda
subdir-$(CONFIG_INTERMEZZO_FS)	+= intermezzo
subdir-$(CONFIG_MINIX_FS)	+= minix
subdir-$(CONFIG_FAT_FS)		+= fat
subdir-$(CONFIG_UMSDOS_FS)	+= umsdos
subdir-$(CONFIG_MSDOS_FS)	+= msdos
subdir-$(CONFIG_VFAT_FS)	+= vfat
subdir-$(CONFIG_BFS_FS)		+= bfs
subdir-$(CONFIG_ISO9660_FS)	+= isofs
subdir-$(CONFIG_DEVFS_FS)	+= devfs
subdir-$(CONFIG_HFSPLUS_FS)	+= hfsplus # Before hfs to find wrapped HFS+
subdir-$(CONFIG_HFS_FS)		+= hfs
subdir-$(CONFIG_VXFS_FS)	+= freevxfs
subdir-$(CONFIG_NFS_FS)		+= nfs
subdir-$(CONFIG_NFSD)		+= nfsd
subdir-$(CONFIG_LOCKD)		+= lockd
subdir-$(CONFIG_NLS)		+= nls
subdir-$(CONFIG_SYSV_FS)	+= sysv
subdir-$(CONFIG_SMB_FS)		+= smbfs
subdir-$(CONFIG_NCP_FS)		+= ncpfs
subdir-$(CONFIG_HPFS_FS)	+= hpfs
subdir-$(CONFIG_NTFS_FS)	+= ntfs
subdir-$(CONFIG_UFS_FS)		+= ufs
subdir-$(CONFIG_EFS_FS)		+= efs
subdir-$(CONFIG_JFFS_FS)	+= jffs
subdir-$(CONFIG_JFFS2_FS)	+= jffs2
subdir-$(CONFIG_AFFS_FS)	+= affs
subdir-$(CONFIG_ROMFS_FS)	+= romfs
subdir-$(CONFIG_QNX4FS_FS)	+= qnx4
subdir-$(CONFIG_UDF_FS)		+= udf
subdir-$(CONFIG_AUTOFS_FS)	+= autofs
subdir-$(CONFIG_AUTOFS4_FS)	+= autofs4
subdir-$(CONFIG_ADFS_FS)	+= adfs
subdir-$(CONFIG_REISERFS_FS)	+= reiserfs
subdir-$(CONFIG_DEVPTS_FS)	+= devpts
subdir-$(CONFIG_SUN_OPENPROMFS)	+= openpromfs
subdir-$(CONFIG_BEFS_FS)	+= befs
subdir-$(CONFIG_JFS_FS)		+= jfs
subdir-$(CONFIG_XFS_FS)		+= xfs

obj-$(CONFIG_BINFMT_AOUT)	+= binfmt_aout.o
obj-$(CONFIG_BINFMT_EM86)	+= binfmt_em86.o
obj-$(CONFIG_BINFMT_MISC)	+= binfmt_misc.o

# binfmt_script is always there
obj-y				+= binfmt_script.o

obj-$(CONFIG_BINFMT_ELF)	+= binfmt_elf.o

# persistent filesystems
obj-y += $(join $(subdir-y),$(subdir-y:%=/%.o))


include $(TOPDIR)/Rules.make
