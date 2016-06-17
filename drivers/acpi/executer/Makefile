#
# Makefile for all Linux ACPI interpreter subdirectories
#

O_TARGET := $(notdir $(CURDIR)).o

obj-$(CONFIG_ACPI_INTERPRETER) := $(patsubst %.c,%.o,$(wildcard *.c))

EXTRA_CFLAGS += $(ACPI_CFLAGS)

include $(TOPDIR)/Rules.make
