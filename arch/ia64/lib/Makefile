#
# Makefile for ia64-specific library routines..
#

.S.o:
	$(CC) $(AFLAGS) $(AFLAGS_KERNEL) -c $< -o $@

L_TARGET = lib.a

export-objs := io.o swiotlb.o

obj-y := __divsi3.o __udivsi3.o __modsi3.o __umodsi3.o					\
	__divdi3.o __udivdi3.o __moddi3.o __umoddi3.o					\
	carta_random.o checksum.o clear_page.o csum_partial_copy.o					\
	clear_user.o strncpy_from_user.o strlen_user.o strnlen_user.o			\
	flush.o ip_fast_csum.o io.o do_csum.o						\
	memset.o strlen.o swiotlb.o

obj-$(CONFIG_ITANIUM) += copy_page.o copy_user.o memcpy.o
obj-$(CONFIG_MCKINLEY) += copy_page_mck.o memcpy_mck.o
obj-$(CONFIG_MD_RAID5) += xor.o

IGNORE_FLAGS_OBJS =	__divsi3.o __udivsi3.o __modsi3.o __umodsi3.o \
			__divdi3.o __udivdi3.o __moddi3.o __umoddi3.o

$(L_TARGET): $(obj-y) $(export-objs)

__divdi3.o: idiv64.S
	$(CC) $(AFLAGS) $(AFLAGS_KERNEL) -c -o $@ $<

__udivdi3.o: idiv64.S
	$(CC) $(AFLAGS) $(AFLAGS_KERNEL) -c -DUNSIGNED -c -o $@ $<

__moddi3.o: idiv64.S
	$(CC) $(AFLAGS) $(AFLAGS_KERNEL) -c -DMODULO -c -o $@ $<

__umoddi3.o: idiv64.S
	$(CC) $(AFLAGS) $(AFLAGS_KERNEL) -c -DMODULO -DUNSIGNED -c -o $@ $<

__divsi3.o: idiv32.S
	$(CC) $(AFLAGS) $(AFLAGS_KERNEL) -c -o $@ $<

__udivsi3.o: idiv32.S
	$(CC) $(AFLAGS) $(AFLAGS_KERNEL) -c -DUNSIGNED -c -o $@ $<

__modsi3.o: idiv32.S
	$(CC) $(AFLAGS) $(AFLAGS_KERNEL) -c -DMODULO -c -o $@ $<

__umodsi3.o: idiv32.S
	$(CC) $(AFLAGS) $(AFLAGS_KERNEL) -c -DMODULO -DUNSIGNED -c -o $@ $<

include $(TOPDIR)/Rules.make
