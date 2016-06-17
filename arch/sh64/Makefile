#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2000, 2001  Paolo Alberelli
# Copyright (C) 2003  Paul Mundt
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies. Remember to do have actions
# for "archclean" and "archdep" for cleaning up and making dependencies for
# this architecture
#
# Note that top level Makefile automagically builds dependencies for SUBDIRS
# but does not automagically clean SUBDIRS. Therefore "archclean" should clean
# up all, "archdep" does nothing on added SUBDIRS.
#

cpu-y				:= -mb
cpu-$(CONFIG_LITTLE_ENDIAN)	:= -ml

cpu-$(CONFIG_CPU_SH5)		+= -m5-32media-nofpu

ifdef CONFIG_LITTLE_ENDIAN
LDFLAGS		+= -EL  -mshlelf32_linux
else
LDFLAGS		+= -EB  -mshelf32_linux
endif

# No requirements for endianess support from AFLAGS, 'as' always run through gcc
AFLAGS		+= -m5 -isa=sh64 -traditional
CFLAGS		+= $(cpu-y)

ENTRY_POINT	= --defsym phys_stext=_stext-0x$(CONFIG_CACHED_MEMORY_OFFSET) \
		  -e phys_stext
OBJCOPY		= $(CROSS_COMPILE)objcopy -O binary -R .note -R .comment \
		  -R .stab -R .stabstr -S
LD		= $(CROSS_COMPILE)ld $(LDFLAGS)

LINKFLAGS     += -T arch/$(ARCH)/vmlinux.lds  $(ENTRY_POINT)

ifdef LOADADDR
LINKFLAGS     += -Ttext $(word 1,$(LOADADDR))
endif

machine-$(CONFIG_SH_CAYMAN)	:= cayman
machine-$(CONFIG_SH_SIMULATOR)	:= sim
machine-$(CONFIG_SH_HARP)	:= harp

HEAD := arch/$(ARCH)/kernel/head.o arch/$(ARCH)/kernel/init_task.o
MAKEROOTFS =

SUBDIRS += $(addprefix arch/$(ARCH)/, kernel mm lib mach-$(machine-y))
CORE_FILES := arch/$(ARCH)/kernel/kernel.o arch/$(ARCH)/mm/mm.o \
	      arch/$(ARCH)/mach-$(machine-y)/$(machine-y).o \
	      $(CORE_FILES) 

LIBGCC := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)
LIBS := $(TOPDIR)/arch/$(ARCH)/lib/lib.a $(LIBS) $(LIBGCC)

MAKEBOOT = $(MAKE) -C arch/$(ARCH)/boot

#
# override the assembly rules
#
#.S.o:
#	$(CC) $(CFLAGS) $(AFLAGS) -c $< -o $*.o

vmlinux: arch/$(ARCH)/vmlinux.lds

arch/$(ARCH)/vmlinux.lds: arch/$(ARCH)/vmlinux.lds.S FORCE
	$(CPP) $(TOOLCHAIN) $(AFLAGS) -C -P -I$(HPATH) -U$(ARCH) arch/$(ARCH)/vmlinux.lds.S >arch/$(ARCH)/vmlinux.lds

FORCE: ;

zImage: vmlinux
	@$(MAKEBOOT) zImage

compressed: zImage

zdisk: vmlinux
	@$(MAKEBOOT) zdisk

archclean:
#	@$(MAKEBOOT) clean

archmrproper:
	rm -f arch/$(ARCH)/vmlinux.lds

archdep:
#	@$(MAKEBOOT) dep
