# Rules for making the NTFS driver

O_TARGET := ntfs.o

obj-y   := fs.o sysctl.o support.o util.o inode.o dir.o super.o attr.o unistr.o
obj-m   := $(O_TARGET)
# New version format started 3 February 2001.
EXTRA_CFLAGS = -DNTFS_VERSION=\"1.1.22\" #-DDEBUG

include $(TOPDIR)/Rules.make

