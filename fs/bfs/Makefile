#
# Makefile for BFS filesystem.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main Makefile...

O_TARGET := bfs.o

obj-y   := inode.o file.o dir.o
obj-m   := $(O_TARGET)

include $(TOPDIR)/Rules.make
