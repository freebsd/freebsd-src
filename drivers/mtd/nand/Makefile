#
# linux/drivers/nand/Makefile
#
# $Id: Makefile,v 1.10 2002/12/01 13:23:05 gleixner Exp $

O_TARGET	:= nandlink.o

export-objs	:= nand.o nand_ecc.o nand_ids.o

obj-$(CONFIG_MTD_NAND)		+= nand.o nand_ecc.o
obj-$(CONFIG_MTD_NAND_SPIA)	+= spia.o
obj-$(CONFIG_MTD_NAND_AUTCPU12)	+= autcpu12.o
obj-$(CONFIG_MTD_NAND_EDB7312)  += edb7312.o
obj-$(CONFIG_MTD_NAND_IDS)	+= nand_ids.o

include $(TOPDIR)/Rules.make
