#
# MAKEFILE FOR THE MIPS LINUX BOOTLOADER AND ROM DEBUGGER
#
# i-data Networks
#
# Author: Thomas Horsten <thh@i-data.com>
#

ifndef Version
 Version = "$(USER)-test"
endif

MKLASATIMG = mklasatimg
MKLASATIMG_ARCH = mq2,mqpro,sp100,sp200
ifdef CONFIG_LASAT_SERVICE
MKLASATIMG_FLAG = -s
else
MKLASATIMG_FLAG = -k
endif

KERNEL_IMAGE = $(TOPDIR)/vmlinux
KERNEL_START = $(shell $(NM) $(KERNEL_IMAGE) | grep _ftext | cut -f1 -d\ )
KERNEL_ENTRY = $(shell $(NM) $(KERNEL_IMAGE) | grep kernel_entry | cut -f1 -d\ )

LDSCRIPT= -Tromscript.normal

AFLAGS_head.o = -D_kernel_start=0x$(KERNEL_START) \
		-D_kernel_entry=0x$(KERNEL_ENTRY) \
		-D VERSION="\"$(Version)\"" \
		-D TIMESTAMP=$(shell date +%s) 

head.o: $(KERNEL_IMAGE)

OBJECTS= head.o kImage.o

rom.sw:	rom.bin
	$(MKLASATIMG) -o rom.sw $(MKLASATIMG_FLAG) rom.bin -m $(MKLASATIMG_ARCH)

rom.bin: rom
	$(OBJCOPY) -O binary -S rom rom.bin

# Rule to make the bootloader
rom:	$(OBJECTS)
	$(LD) $(LDFLAGS) $(LDSCRIPT) -o rom $(OBJECTS) 

%.o: %.gz
	$(LD) -r -o $@ -b binary $<

%.gz: %.bin
	gzip -cf -9 $< > $@

kImage.bin: $(KERNEL_IMAGE)
	$(OBJCOPY) -O binary -S $(TOPDIR)/vmlinux kImage.bin

clean:
	rm -f rom rom.bin rom.sw kImage.bin

USE_STANDARD_AS_RULE = 1
include $(TOPDIR)/Rules.make
