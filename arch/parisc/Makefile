#
# parisc/Makefile
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies. Remember to do have actions
# for "archclean" and "archdep" for cleaning up and making dependencies for
# this architecture
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1994 by Linus Torvalds
# Portions Copyright (C) 1999 The Puffin Group
#
# Modified for PA-RISC Linux by Paul Lahaie, Alex deVries, 
# Mike Shaver, Helge Deller and Martin K. Petersen
#

ifdef CONFIG_PARISC64
LINKFLAGS =-T $(TOPDIR)/arch/parisc/vmlinux64.lds $(LDFLAGS)
CROSS_COMPILE := hppa64-linux-
UTS_MACHINE := parisc64
#CFLAGS += -b hppa64-linux

else
LINKFLAGS =-T $(TOPDIR)/arch/parisc/vmlinux.lds $(LDFLAGS)
MACHINE	:= $(subst 64,,$(shell uname -m))
ifneq (${MACHINE},parisc)
# cross compilation
CROSS_COMPILE := hppa-linux-
endif
endif

OFFSET_H := $(TOPDIR)/include/asm-parisc/offset.h
FINAL_LD=$(CROSS_COMPILE)ld --warn-common --warn-section-align 

CPP=$(CC) -E
OBJCOPY=$(CROSS_COMPILE)objcopy -O binary -R .note -R .comment -S
LDFLAGS =


CFLAGS_PIPE := -pipe
CFLAGS_NSR  := -fno-strength-reduce
CFLAGS := $(CFLAGS) -D__linux__ $(CFLAGS_PIPE) $(CFLAGS_NSR)

# These should be on for older toolchains or SOM toolchains that don't
# enable them by default.
CFLAGS += -mno-space-regs -mfast-indirect-calls

# No fixed-point multiply
CFLAGS += -mdisable-fpregs

# Without this, "ld -r" results in .text sections that are too big
# (> 0x40000) for branches to reach stubs.
CFLAGS += -ffunction-sections

# select which processor to optimise for
ifdef CONFIG_PA7100
CFLAGS += -march=1.1 -mschedule=7100
endif

ifdef CONFIG_PA7200
CFLAGS += -march=1.1 -mschedule=7200
endif

ifdef CONFIG_PA7100LC
CFLAGS += -march=1.1 -mschedule=7100LC
endif

ifdef CONFIG_PA8X00
CFLAGS += -march=2.0 -mschedule=8000
endif

ASFLAGS := -D__ASSEMBLY__ -traditional

HEAD := arch/parisc/kernel/head.o 

SUBDIRS := $(SUBDIRS) $(addprefix arch/parisc/, tools kernel mm lib hpux)
CORE_FILES :=  $(addprefix arch/parisc/, kernel/pdc_cons.o kernel/process.o \
	lib/lib.a mm/mm.o kernel/kernel.o hpux/hpux.o) \
	$(CORE_FILES) arch/parisc/kernel/init_task.o
CLEAN_FILES := $(CLEAN_FILES) arch/parisc/tools/offset.[hs] 

ifdef CONFIG_PARISC64
HEAD := arch/parisc/kernel/head64.o
endif

LIBS := `$(CC) -print-libgcc-file-name` $(TOPDIR)/arch/parisc/lib/lib.a $(LIBS) 

SUBDIRS := $(SUBDIRS) arch/parisc/math-emu
DRIVERS := $(DRIVERS) arch/parisc/math-emu/math.o

arch/parisc/kernel: dummy
	$(MAKE) linuxsubdirs SUBDIRS=arch/parisc/kernel

arch/parisc/mm: dummy
	$(MAKE) linuxsubdirs SUBDIRS=arch/parisc/mm

palo: vmlinux
	@if [ $$(palo -f /dev/null >/dev/null 2>&1 ; echo $$?) != 2 ]; then \
		echo 'ERROR: Please install palo first (apt-get install palo)';\
		echo 'or build it from source and install it somewhere in your $$PATH';\
		false; \
	fi
	@if [ ! -f ./palo.conf ]; then \
		cp arch/parisc/defpalo.conf palo.conf; \
		echo 'A generic palo config file (./palo.conf) has been created for you.'; \
		echo 'You should check it and re-run "make palo".'; \
		echo 'WARNING: the "lifimage" file is now placed in this directory by default!'; \
		false; \
	fi
	palo -f ./palo.conf

oldpalo: vmlinux
	export TOPDIR=`pwd`; \
	unset STRIP LDFLAGS CPP CPPFLAGS AFLAGS CFLAGS CC LD; cd ../palo && make lifimage

Image: palo

Image-clean:

ramdisk.o:

zImage: palo

bzImage: palo

compressed: zImage

install: 

archmrproper:
	rm -f $(OFFSET_H)

archclean:

archdep: $(OFFSET_H)

$(OFFSET_H): include/linux/version.h
	$(MAKE) -C $(TOPDIR)/arch/$(ARCH)/tools all

.PHONY: $(OFFSET_H)
