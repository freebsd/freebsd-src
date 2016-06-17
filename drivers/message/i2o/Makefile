#
# Makefile for the kernel I2O OSM.
#
# Note : at this point, these files are compiled on all systems.
# In the future, some of these should be built conditionally.
#

O_TARGET := i2o.o

export-objs	:= i2o_pci.o i2o_core.o i2o_config.o i2o_block.o i2o_lan.o i2o_scsi.o i2o_proc.o

#
# Do not touch the order here 
# We must link i2o_core, then i2o_pci, then protocols
#
obj-$(CONFIG_I2O)	+= i2o_core.o
obj-$(CONFIG_I2O_PCI)	+= i2o_pci.o
obj-$(CONFIG_I2O)	+= i2o_config.o
obj-$(CONFIG_I2O_BLOCK)	+= i2o_block.o
obj-$(CONFIG_I2O_LAN)	+= i2o_lan.o
obj-$(CONFIG_I2O_SCSI)	+= i2o_scsi.o
obj-$(CONFIG_I2O_PROC)	+= i2o_proc.o

include $(TOPDIR)/Rules.make

