#
# Makefile for a ramdisk image
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

O_FORMAT = $(shell $(OBJDUMP) -i | head -2 | grep elf32)
img = $(CONFIG_EMBEDDED_RAMDISK_IMAGE)
ramdisk.o: $(subst ",,$(img)) ld.script
	echo "O_FORMAT:  " $(O_FORMAT)
	$(LD) -T ld.script -b binary --oformat $(O_FORMAT) -o $@ $(img)

include $(TOPDIR)/Rules.make
