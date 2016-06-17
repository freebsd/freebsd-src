#
# Makefile for the linux autofs-filesystem routines.
#
# We can build this either out of the kernel tree or the autofs tools tree.
#

O_TARGET := autofs.o

obj-y   := dir.o dirhash.o init.o inode.o root.o symlink.o waitq.o
obj-m   := $(O_TARGET)

include $(TOPDIR)/Rules.make
