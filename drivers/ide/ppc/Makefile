
O_TARGET := idedriver-ppc.o

obj-y		:=
obj-m		:=

obj-$(CONFIG_BLK_DEV_CPCI405_IDE)	+= cpci405ide.o
obj-$(CONFIG_BLK_DEV_MPC8xx_IDE)	+= mpc8xx.o
obj-$(CONFIG_BLK_DEV_IDE_PMAC)		+= pmac.o
obj-$(CONFIG_BLK_DEV_IDE_SWARM)		+= swarm.o

EXTRA_CFLAGS	:= -I../

include $(TOPDIR)/Rules.make
