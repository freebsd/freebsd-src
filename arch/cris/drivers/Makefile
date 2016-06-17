#
# Makefile for Etrax-specific drivers
#

O_TARGET := drivers.o

export-objs := axisflashmap.o

obj-y :=

obj-$(CONFIG_ETRAX_VIRTEX_FPGA)         += virtex.o
obj-$(CONFIG_ETRAX_ETHERNET)            += ethernet.o
obj-$(CONFIG_ETRAX_SERIAL)              += serial.o
obj-$(CONFIG_ETRAX_IDE)                 += ide.o
obj-$(CONFIG_ETRAX_AXISFLASHMAP)        += axisflashmap.o
obj-$(CONFIG_ETRAX_I2C) 	        += i2c.o
obj-$(CONFIG_ETRAX_I2C_EEPROM)          += eeprom.o
obj-$(CONFIG_ETRAX_GPIO) 	        += gpio.o
obj-$(CONFIG_ETRAX_USB_HOST)            += usb-host.o
obj-$(CONFIG_ETRAX_SYNCHRONOUS_SERIAL)  += sync_serial.o
obj-$(CONFIG_ETRAX_PARPORT)             += parport.o
obj-$(CONFIG_ETRAX_DS1302)              += ds1302.o
obj-$(CONFIG_ETRAX_PCF8563)		+= pcf8563.o


obj-$(CONFIG_ETRAX_ETHERNET_LPSLAVE)    += lpslave/lpslavedrivers.o
subdir-$(CONFIG_ETRAX_ETHERNET_LPSLAVE) += lpslave

include $(TOPDIR)/Rules.make

