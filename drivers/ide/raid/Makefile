
O_TARGET := idedriver-raid.o

obj-y		:=
obj-m		:=

export-objs := ataraid.o

# The virtualised raid layers MUST come after the ide itself or bad stuff
# will happen.

obj-$(CONFIG_BLK_DEV_ATARAID)		+= ataraid.o
obj-$(CONFIG_BLK_DEV_ATARAID_PDC)	+= pdcraid.o
obj-$(CONFIG_BLK_DEV_ATARAID_HPT)	+= hptraid.o
obj-$(CONFIG_BLK_DEV_ATARAID_MEDLEY)	+= medley.o
obj-$(CONFIG_BLK_DEV_ATARAID_SII)	+= silraid.o

EXTRA_CFLAGS	:= -I../

include $(TOPDIR)/Rules.make
