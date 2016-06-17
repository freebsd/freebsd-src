# File: drivers/acorn/net/Makefile
#
# Makefile for the Acorn ethercard network device drivers
#

O_TARGET := acorn-net.o

obj-y	:=
obj-m	:=
obj-n	:=
obj-	:=

obj-$(CONFIG_ARM_ETHERH)	+= etherh.o
obj-$(CONFIG_ARM_ETHER3)	+= ether3.o
obj-$(CONFIG_ARM_ETHER1)	+= ether1.o

include $(TOPDIR)/Rules.make
