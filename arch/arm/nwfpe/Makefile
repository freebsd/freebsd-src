#
# linux/arch/arm/nwfpe/Makefile
#
# Copyright (C) 1998, 1999, 2001 Philip Blundell
#

USE_STANDARD_AS_RULE := true

O_TARGET		:= math-emu.o

obj-y			:=
obj-m			:=
obj-n			:=

list-multi		:= nwfpe.o

obj-$(CONFIG_FPE_NWFPE)	+= nwfpe.o

nwfpe-objs		:= fpa11.o fpa11_cpdo.o fpa11_cpdt.o fpa11_cprt.o \
			   fpmodule.o fpopcode.o softfloat.o \
			   single_cpdo.o double_cpdo.o

ifeq ($(CONFIG_FPE_NWFPE_XP),y)
nwfpe-objs		+= extended_cpdo.o
endif

ifeq ($(CONFIG_CPU_26),y)
nwfpe-objs		+= entry26.o
else
nwfpe-objs		+= entry.o
endif

include $(TOPDIR)/Rules.make

nwfpe.o: $(nwfpe-objs)
	$(LD) -r -o $@ $(nwfpe-objs)
