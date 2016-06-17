# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := decnet.o
obj-y    := af_decnet.o dn_nsp_in.o dn_nsp_out.o dn_route.o dn_dev.o dn_neigh.o dn_timer.o
obj-m	 := $(O_TARGET)

obj-$(CONFIG_DECNET_ROUTER) += dn_fib.o dn_rules.o dn_table.o
obj-$(CONFIG_DECNET_FW) += dn_fw.o

obj-y += sysctl_net_decnet.o

include $(TOPDIR)/Rules.make

