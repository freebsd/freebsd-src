#
# Makefile for the Intel's E100 ethernet driver
#

O_TARGET := e100.o

obj-y	 :=	e100_main.o e100_config.o e100_phy.o \
		e100_eeprom.o e100_test.o
obj-m	 := $(O_TARGET)

include $(TOPDIR)/Rules.make
