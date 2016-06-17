#
# Makefile for the Linux IrDA protocol layer.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := irda.o

export-objs := irsyms.o

obj-y	 := iriap.o iriap_event.o irlmp.o irlmp_event.o irlmp_frame.o \
            irlap.o irlap_event.o irlap_frame.o timer.o qos.o irqueue.o \
            irttp.o irda_device.o irias_object.o crc.o wrapper.o af_irda.o \
	    discovery.o parameters.o irsyms.o

ifeq ($(CONFIG_IRDA),m)
obj-m   := $(O_TARGET)
endif

obj-$(CONFIG_PROC_FS) += irproc.o
obj-$(CONFIG_SYSCTL) += irsysctl.o
obj-$(CONFIG_IRLAN) += irlan/irlan.o

subdir-$(CONFIG_IRLAN) += irlan
subdir-$(CONFIG_IRNET) += irnet
subdir-$(CONFIG_IRCOMM) += ircomm

ifeq ($(CONFIG_IRLAN),y)
obj-y += irlan/irlan.o
endif

ifeq ($(CONFIG_IRNET),y)
obj-y += irnet/irnet.o
endif

ifeq ($(CONFIG_IRCOMM),y)
obj-y += ircomm/ircomm_and_tty.o
endif

include $(TOPDIR)/Rules.make
