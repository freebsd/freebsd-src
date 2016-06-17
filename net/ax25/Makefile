#
# Makefile for the Linux AX.25 layer.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...


O_TARGET := ax25.o

export-objs := af_ax25.o

obj-y	 := ax25_addr.o ax25_dev.o ax25_iface.o ax25_in.o ax25_ip.o ax25_out.o \
	    ax25_route.o ax25_std_in.o ax25_std_subr.o ax25_std_timer.o \
	    ax25_subr.o ax25_timer.o ax25_uid.o af_ax25.o

obj-m    := $(O_TARGET)

obj-$(CONFIG_AX25_DAMA_SLAVE) += ax25_ds_in.o ax25_ds_subr.o ax25_ds_timer.o
obj-$(CONFIG_SYSCTL) += sysctl_net_ax25.o

include $(TOPDIR)/Rules.make

