#
# Makefile for parisc-specific library files..
#


L_TARGET = lib.a
obj-y	:= lusercopy.o bitops.o checksum.o io.o memset.o

obj-$(CONFIG_SMP) += locks.o

include $(TOPDIR)/Rules.make
