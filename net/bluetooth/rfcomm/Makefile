#
# Makefile for the Linux Bluetooth RFCOMM layer
#

O_TARGET := rfcomm.o

obj-y				:= core.o sock.o crc.o
obj-$(CONFIG_BLUEZ_RFCOMM_TTY)	+= tty.o
obj-m				+= $(O_TARGET)

include $(TOPDIR)/Rules.make
