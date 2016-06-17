#
# m68k/Makefile
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
# Copyright (C) 1994 by Hamish Macdonald
#

# test for cross compiling
COMPILE_ARCH = $(shell uname -m)

# override top level makefile
AS += -m68020
LD += -m m68kelf
ifneq ($(COMPILE_ARCH),$(ARCH))
	# prefix for cross-compiling binaries
	CROSS_COMPILE = m68k-linux-
endif

ifndef CONFIG_SUN3
LINKFLAGS = -T $(TOPDIR)/arch/m68k/vmlinux.lds
else
LINKFLAGS = -T $(TOPDIR)/arch/m68k/vmlinux-sun3.lds -N
endif

# without -fno-strength-reduce the 53c7xx.c driver fails ;-(
CFLAGS += -pipe -fno-strength-reduce -ffixed-a2

# enable processor switch if compiled only for a single cpu
ifndef CONFIG_M68020
ifndef CONFIG_M68030

ifndef CONFIG_M68060
CFLAGS := $(CFLAGS) -m68040
endif

ifndef CONFIG_M68040
CFLAGS := $(CFLAGS) -m68060
endif

endif
endif

ifdef CONFIG_KGDB
# If configured for kgdb support, include debugging infos and keep the
# frame pointer
CFLAGS := $(subst -fomit-frame-pointer,,$(CFLAGS)) -g
endif

ifndef CONFIG_SUN3
HEAD := arch/m68k/kernel/head.o
else
HEAD := arch/m68k/kernel/sun3-head.o
endif 

SUBDIRS += arch/m68k/kernel arch/m68k/mm arch/m68k/lib
CORE_FILES := arch/m68k/kernel/kernel.o arch/m68k/mm/mm.o $(CORE_FILES)
LIBS += arch/m68k/lib/lib.a

ifdef CONFIG_Q40
CORE_FILES := $(CORE_FILES) arch/m68k/q40/q40.o
SUBDIRS := $(SUBDIRS) arch/m68k/q40
endif

ifdef CONFIG_AMIGA
CORE_FILES := $(CORE_FILES) arch/m68k/amiga/amiga.o
SUBDIRS := $(SUBDIRS) arch/m68k/amiga
endif

ifdef CONFIG_ATARI
CORE_FILES := $(CORE_FILES) arch/m68k/atari/atari.o
SUBDIRS := $(SUBDIRS) arch/m68k/atari
endif

ifdef CONFIG_MAC
CORE_FILES := $(CORE_FILES) arch/m68k/mac/mac.o
SUBDIRS := $(SUBDIRS) arch/m68k/mac
endif

ifdef CONFIG_HP300
CORE_FILES := $(CORE_FILES) arch/m68k/hp300/hp300.o
SUBDIRS := $(SUBDIRS) arch/m68k/hp300
endif

ifdef CONFIG_APOLLO
CORE_FILES := $(CORE_FILES) arch/m68k/apollo/apollo.o
SUBDIRS := $(SUBDIRS) arch/m68k/apollo
endif

ifdef CONFIG_MVME147
CORE_FILES := $(CORE_FILES) arch/m68k/mvme147/mvme147.o
SUBDIRS := $(SUBDIRS) arch/m68k/mvme147
endif

ifdef CONFIG_MVME16x
CORE_FILES := $(CORE_FILES) arch/m68k/mvme16x/mvme16x.o
SUBDIRS := $(SUBDIRS) arch/m68k/mvme16x
endif

ifdef CONFIG_BVME6000
CORE_FILES := $(CORE_FILES) arch/m68k/bvme6000/bvme6000.o
SUBDIRS := $(SUBDIRS) arch/m68k/bvme6000
endif

ifdef CONFIG_SUN3X
CORE_FILES := $(CORE_FILES) arch/m68k/sun3x/sun3x.o arch/m68k/sun3/sun3.o
SUBDIRS := $(SUBDIRS) arch/m68k/sun3x arch/m68k/sun3
endif

ifdef CONFIG_SUN3
CORE_FILES := $(CORE_FILES) arch/m68k/sun3/sun3.o arch/m68k/sun3/prom/promlib.a
SUBDIRS := $(SUBDIRS) arch/m68k/sun3 arch/m68k/sun3/prom
endif

ifdef CONFIG_M68040
CORE_FILES := $(CORE_FILES) arch/m68k/fpsp040/fpsp.o
SUBDIRS := $(SUBDIRS) arch/m68k/fpsp040
endif

ifdef CONFIG_M68060
CORE_FILES := $(CORE_FILES) arch/m68k/ifpsp060/ifpsp.o
SUBDIRS := $(SUBDIRS) arch/m68k/ifpsp060
endif

ifdef CONFIG_M68KFPU_EMU
CORE_FILES := $(CORE_FILES) arch/m68k/math-emu/mathemu.o
SUBDIRS := $(SUBDIRS) arch/m68k/math-emu
endif

lilo:	vmlinux
	if [ -f $(INSTALL_PATH)/vmlinux ]; then mv -f $(INSTALL_PATH)/vmlinux $(INSTALL_PATH)/vmlinux.old; fi
	if [ -f $(INSTALL_PATH)/System.map ]; then mv -f $(INSTALL_PATH)/System.map $(INSTALL_PATH)/System.old; fi
	cat vmlinux > $(INSTALL_PATH)/vmlinux
	cp System.map $(INSTALL_PATH)/System.map
	if [ -x /sbin/lilo ]; then /sbin/lilo; else /etc/lilo/install; fi

zImage compressed: vmlinux.gz

vmlinux.gz: vmlinux

ifndef CONFIG_KGDB
	cp vmlinux vmlinux.tmp
	$(STRIP) vmlinux.tmp
	gzip -9c vmlinux.tmp >vmlinux.gz
	rm vmlinux.tmp
else
	gzip -9c vmlinux >vmlinux.gz
endif

bzImage: vmlinux.bz2

vmlinux.bz2: vmlinux

ifndef CONFIG_KGDB
	cp vmlinux vmlinux.tmp
	$(STRIP) vmlinux.tmp
	bzip2 -1c vmlinux.tmp >vmlinux.bz2
	rm vmlinux.tmp
else
	bzip2 -1c vmlinux >vmlinux.bz2
endif

archclean:
	rm -f vmlinux.gz vmlinux.bz2
	rm -f arch/m68k/kernel/m68k_defs.h arch/m68k/kernel/m68k_defs.d

archmrproper:

archdep:
