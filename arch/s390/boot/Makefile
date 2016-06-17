#
# arch/s390/boot/Makefile
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

OBJCOPY = $(CROSS_COMPILE)objcopy

O_TARGET := 

include $(TOPDIR)/Rules.make

.S.o:
	$(CC) -D__ASSEMBLY__ $(AFLAGS) -traditional -c $< -o $*.o

%.lnk: %.o
	$(LD) -Ttext 0x0 -o $@ $<

%.boot: %.lnk
	$(OBJCOPY) -O binary $< $@

image: $(CONFIGURE) $(TOPDIR)/vmlinux \
	iplfba.boot ipleckd.boot ipldump.boot
	$(OBJCOPY) -O binary $(TOPDIR)/vmlinux image
	$(NM) $(TOPDIR)/vmlinux | grep -v '\(compiled\)\|\( [aUw] \)\|\(\.\)\|\(LASH[RL]DI\)' | sort > $(TOPDIR)/System.map

listing: ../../../vmlinux
	$(OBJDUMP) --disassemble --disassemble-all --disassemble-zeroes --reloc $(TOPDIR)/vmlinux > listing

dep:

clean:
	rm -f image listing iplfba.boot ipleckd.boot ipldump.boot

install: $(CONFIGURE) $(BOOTIMAGE)
	sh -x ./install.sh $(KERNELRELEASE) $(BOOTIMAGE) $(TOPDIR)/System.map $(TOPDIR)/Kerntypes "$(INSTALL_PATH)"

