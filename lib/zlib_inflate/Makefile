#
# This is a modified version of zlib, which does all memory
# allocation ahead of time.
#
# This is only the decompression, see zlib_deflate for the
# the compression
#
# Decompression needs to be serialized for each memory
# allocation.
#
# (The upsides of the simplification is that you can't get in
# any nasty situations wrt memory management, and that the
# uncompression can be done without blocking on allocation).
#

O_TARGET    := zlib_inflate.o

export-objs := inflate_syms.o

obj-y := infblock.o infcodes.o inffast.o inflate.o \
	 inftrees.o infutil.o inflate_syms.o
obj-m := $(O_TARGET)

EXTRA_CFLAGS += -I $(TOPDIR)/lib/zlib_inflate

include $(TOPDIR)/Rules.make
