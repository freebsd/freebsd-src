#
# This is a modified version of zlib, which does all memory
# allocation ahead of time.
#
# This is the compression code, see zlib_inflate for the
# decompression code.
#

O_TARGET    := zlib_deflate.o

export-objs := deflate_syms.o

obj-y := deflate.o deftree.o deflate_syms.o
obj-m := $(O_TARGET)

EXTRA_CFLAGS += -I $(TOPDIR)/lib/zlib_deflate

include $(TOPDIR)/Rules.make
