
O_TARGET := idedriver-legacy.o

obj-y		:=
obj-m		:=

obj-$(CONFIG_BLK_DEV_ALI14XX)		+= ali14xx.o
obj-$(CONFIG_BLK_DEV_DTC2278)		+= dtc2278.o
obj-$(CONFIG_BLK_DEV_HT6560B)		+= ht6560b.o
obj-$(CONFIG_BLK_DEV_PDC4030)		+= pdc4030.o
obj-$(CONFIG_BLK_DEV_QD65XX)		+= qd65xx.o
obj-$(CONFIG_BLK_DEV_UMC8672)		+= umc8672.o

obj-$(CONFIG_BLK_DEV_BUDDHA)		+= buddha.o
obj-$(CONFIG_BLK_DEV_FALCON_IDE)	+= falconide.o
obj-$(CONFIG_BLK_DEV_GAYLE)		+= gayle.o
obj-$(CONFIG_BLK_DEV_MAC_IDE)		+= macide.o
obj-$(CONFIG_BLK_DEV_Q40IDE)		+= q40ide.o

obj-$(CONFIG_BLK_DEV_IDE_SIBYTE)	+= sibyte.o

obj-$(CONFIG_BLK_DEV_IDECS)		+= ide-cs.o


# Last of all
obj-$(CONFIG_BLK_DEV_HD)		+= hd.o

EXTRA_CFLAGS	:= -I../

include $(TOPDIR)/Rules.make
