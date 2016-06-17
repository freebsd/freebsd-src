#
# Makefile for ppc64-specific library files..
#

USE_STANDARD_AS_RULE    := true

O_TARGET = lib.o

export-objs     := dec_and_lock.o

obj-y           := checksum.o dec_and_lock.o string.o strcase.o copypage.o \
		   memcpy.o copyuser.o

include $(TOPDIR)/Rules.make
