# arch/ia64/hp/Makefile
# Copyright (c) 2002 Matthew Wilcox for Hewlett Packard

ALL_SUB_DIRS := sim zx1 common

O_TARGET := hp.o

subdir-$(CONFIG_IA64_GENERIC) += $(ALL_SUB_DIRS)
subdir-$(CONFIG_IA64_HP_SIM) += sim
subdir-$(CONFIG_IA64_HP_ZX1) += zx1 common

SUB_DIRS := $(subdir-y)
obj-y += $(join $(subdir-y),$(subdir-y:%=/%.o))

include $(TOPDIR)/Rules.make
