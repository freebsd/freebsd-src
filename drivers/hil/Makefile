#
# Makefile for the kernel HIL device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#

O_TARGET := hil.o

obj-y	 += 

# All of the (potential) objects that export symbols.
# This list comes from 'grep -l EXPORT_SYMBOL *.[hc]'.
export-objs     :=	hp_sdc.o hil_mlc.o 

mod-subdirs	:=	

list-multi	:=	


obj-$(CONFIG_HIL_KBD_BASIC)	+= hilkbd.o

obj-$(CONFIG_HP_SDC)		+= hp_sdc.o
obj-$(CONFIG_HP_SDC_MLC)	+= hp_sdc_mlc.o
obj-$(CONFIG_HP_SDC_RTC)	+= hp_sdc_rtc.o
obj-$(CONFIG_HIL_MLC)		+= hil_mlc.o

obj-$(CONFIG_HIL_KBD)		+= hil_kbd.o
obj-$(CONFIG_HIL_PTR)		+= hil_ptr.o


include $(TOPDIR)/Rules.make

fastdep:

