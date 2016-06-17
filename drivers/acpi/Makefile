#
# Makefile for the Linux ACPI interpreter
# 

O_TARGET := acpi.o

export ACPI_CFLAGS

ACPI_CFLAGS	:= -Os 

ifdef CONFIG_ACPI_DEBUG
  ACPI_CFLAGS	+= -DACPI_DEBUG_OUTPUT
endif

EXTRA_CFLAGS	+= $(ACPI_CFLAGS)

export-objs 	:= acpi_ksyms.o processor.o

obj-$(CONFIG_ACPI)	:= acpi_ksyms.o 

#
# ACPI Boot-Time Table Parsing
#
obj-$(CONFIG_ACPI_BOOT)		+= tables.o
obj-$(CONFIG_ACPI_INTERPRETER)	+= blacklist.o

#
# ACPI Core Subsystem (Interpreter)
#
ifeq ($(CONFIG_ACPI_INTERPRETER),y)
  obj-y		+= osl.o utils.o
  subdir-y	+= dispatcher events executer hardware namespace parser \
		   resources tables utilities
  obj-y 	+= $(foreach dir,$(subdir-y),$(dir)/$(dir).o)
endif

#
# ACPI Bus and Device Drivers
#
ifeq ($(CONFIG_ACPI_BUS),y)
  obj-y				+= bus.o 
  obj-$(CONFIG_ACPI_AC) 	+= ac.o
  obj-$(CONFIG_ACPI_BATTERY)	+= battery.o
  obj-$(CONFIG_ACPI_BUTTON)	+= button.o
  obj-$(CONFIG_ACPI_EC)		+= ec.o
  obj-$(CONFIG_ACPI_FAN)	+= fan.o
  obj-$(CONFIG_ACPI_PCI)	+= pci_root.o pci_link.o pci_irq.o pci_bind.o
  obj-$(CONFIG_ACPI_POWER)	+= power.o
  obj-$(CONFIG_ACPI_PROCESSOR)	+= processor.o
  obj-$(CONFIG_ACPI_THERMAL)	+= thermal.o
  obj-$(CONFIG_ACPI_SYSTEM)	+= system.o
  obj-$(CONFIG_ACPI_NUMA)	+= numa.o
  obj-$(CONFIG_ACPI_ASUS)	+= asus_acpi.o
  obj-$(CONFIG_ACPI_TOSHIBA)	+= toshiba_acpi.o
endif

include $(TOPDIR)/Rules.make
