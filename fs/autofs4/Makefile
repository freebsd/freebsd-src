#
# Makefile for the linux autofs-filesystem routines.
#
# We can build this either out of the kernel tree or the autofs tools tree.
#

O_TARGET := autofs4.o

obj-y   := init.o inode.o root.o symlink.o waitq.o expire.o

obj-m   := $(O_TARGET)

include $(TOPDIR)/Rules.make
