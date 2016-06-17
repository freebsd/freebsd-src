#
# Makefile for the Linux Ethernet layer.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := ethernet.o

OBJS	:= eth.o

ifeq ($(CONFIG_SYSCTL),y)
OBJS += sysctl_net_ether.o
endif

ifdef CONFIG_IPX
OBJ2	:= pe2.o
endif

ifdef CONFIG_ATALK
OBJ2	:= pe2.o
endif

obj-$(CONFIG_NET)	:= $(OBJS) $(OBJ2)

include $(TOPDIR)/Rules.make
