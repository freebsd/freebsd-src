#
# Makefile for ACP Modem (Mwave).
#
# See the README file in this directory for more info. <paulsch@us.ibm.com>
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#

# To compile in lots (~20 KiB) of run-time enablable printk()s for debugging:
EXTRA_CFLAGS += -DMW_TRACE

# To have the mwave driver disable other uarts if necessary
# EXTRA_CFLAGS += -DMWAVE_FUTZ_WITH_OTHER_DEVICES

O_TARGET := mwave.o

obj-y	:= mwavedd.o smapi.o tp3780i.o 3780i.o
obj-m	:= $(O_TARGET)

include $(TOPDIR)/Rules.make
