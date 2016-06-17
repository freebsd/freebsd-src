 #
# Makefile for some libs needed in the kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

L_TARGET := lib.a

export-objs := cmdline.o dec_and_lock.o rwsem-spinlock.o rwsem.o \
	       rbtree.o crc32.o firmware_class.o

obj-y := errno.o ctype.o string.o vsprintf.o brlock.o cmdline.o \
	 bust_spinlocks.o rbtree.o dump_stack.o

obj-$(CONFIG_FW_LOADER) += firmware_class.o
obj-$(CONFIG_RWSEM_GENERIC_SPINLOCK) += rwsem-spinlock.o
obj-$(CONFIG_RWSEM_XCHGADD_ALGORITHM) += rwsem.o

ifneq ($(CONFIG_HAVE_DEC_LOCK),y) 
  obj-y += dec_and_lock.o
endif

obj-$(CONFIG_CRC32)     += crc32.o

subdir-$(CONFIG_ZLIB_INFLATE) += zlib_inflate
subdir-$(CONFIG_ZLIB_DEFLATE) += zlib_deflate

include $(TOPDIR)/drivers/net/Makefile.lib
include $(TOPDIR)/drivers/usb/Makefile.lib
include $(TOPDIR)/drivers/bluetooth/Makefile.lib
include $(TOPDIR)/fs/Makefile.lib
include $(TOPDIR)/net/bluetooth/bnep/Makefile.lib

# Include the subdirs, if necessary.
obj-y += $(join $(subdir-y),$(subdir-y:%=/%.o))


include $(TOPDIR)/Rules.make

crc32.o: crc32table.h

gen_crc32table: gen_crc32table.c
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<

crc32table.h: gen_crc32table
	./$< > $@
