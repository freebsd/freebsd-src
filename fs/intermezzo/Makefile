#
# Makefile 1.00 Peter Braam <braam@clusterfs.com>
#

O_TARGET := intermezzo.o

obj-y :=  cache.o dcache.o dir.o ext_attr.o file.o fileset.o inode.o \
	  journal.o journal_ext2.o journal_ext3.o journal_obdfs.o \
          journal_reiserfs.o journal_tmpfs.o journal_xfs.o kml_reint.o \
          kml_unpack.o methods.o presto.o psdev.o replicator.o super.o \
          sysctl.o upcall.o vfs.o

obj-m := $(O_TARGET)

include $(TOPDIR)/Rules.make
