#
# Makefile for the linux devfs-filesystem routines.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := devfs.o

export-objs := base.o util.o

obj-y   := base.o util.o

# Special case to support building documentation
ifndef TOPDIR
TOPDIR = ../..
endif

include $(TOPDIR)/Rules.make


# Rule to build documentation
doc:	base.c util.c
	@echo '$$PACKAGE devfs' > devfs.doc
	@echo '$$NAME Linux Kernel' >> devfs.doc
	@echo '$$SUMMARY devfs (Device FileSystem) functions' >> devfs.doc
	@echo '$$SYNOPSIS "#include <linux/devfs_fs.h>"' >> devfs.doc
	@echo '$$END' >> devfs.doc
	c2doc base.c util.c >> devfs.doc
	karma_doc2man -section 9 devfs.doc .
	rm devfs.doc
	gzip --best *.9
	mv *.9.gz /usr/man/man9


# Rule for test compiling
test:
	gcc -o /tmp/base.o -D__KERNEL__ -I../../include -Wall \
	-Wstrict-prototypes -O2 -fomit-frame-pointer -pipe \
	-fno-strength-reduce   -DEXPORT_SYMTAB -c base.c
