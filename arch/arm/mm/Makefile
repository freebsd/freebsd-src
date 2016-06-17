#
# Makefile for the linux arm-specific parts of the memory manager.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

USE_STANDARD_AS_RULE := true

O_TARGET	:= mm.o

# Object file lists.

obj-y		:= init.o extable.o fault-common.o
obj-m		:=
obj-n		:=
obj-		:=
export-objs	:= proc-syms.o discontig.o

ifeq ($(CONFIG_CPU_32),y)
obj-y		+= consistent.o fault-armv.o ioremap.o mm-armv.o
obj-$(CONFIG_MODULES) += proc-syms.o
endif

obj-$(CONFIG_CPU_26) += fault-armo.o mm-armo.o small_page.o

obj-$(CONFIG_ALIGNMENT_TRAP) += alignment.o
obj-$(CONFIG_DISCONTIGMEM) += discontig.o

# Select the processor-specific files
p-$(CONFIG_CPU_26)	+= proc-arm2,3.o
p-$(CONFIG_CPU_ARM610)	+= proc-arm6,7.o
p-$(CONFIG_CPU_ARM710)	+= proc-arm6,7.o
p-$(CONFIG_CPU_ARM720T)	+= proc-arm720.o
p-$(CONFIG_CPU_ARM920T)	+= proc-arm920.o
p-$(CONFIG_CPU_ARM922T)	+= proc-arm922.o
p-$(CONFIG_CPU_ARM925T)	+= proc-arm925.o
p-$(CONFIG_CPU_ARM926T)	+= proc-arm926.o
p-$(CONFIG_CPU_ARM1020)	+= proc-arm1020.o
p-$(CONFIG_CPU_ARM1026)	+= proc-arm1026.o
p-$(CONFIG_CPU_SA110)	+= proc-sa110.o
p-$(CONFIG_CPU_SA1100)	+= proc-sa110.o

# Integrator follows "new style"
# Soon, others will do too, and we can get rid of this
MMMACH		:= mm-$(MACHINE).c
ifeq ($(MMMACH),$(wildcard $(MMMACH)))
obj-$(CONFIG_CPU_32)	+= $(MMMACH:.c=.o)
endif

obj-y		+= $(sort $(p-y))

include $(TOPDIR)/Rules.make

# Special dependencies
$(p-y):	$(TOPDIR)/include/asm-arm/constants.h
