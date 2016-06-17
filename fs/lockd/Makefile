#
# Makefile for the linux lock manager stuff
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := lockd.o

export-objs := lockd_syms.o

obj-y    := clntlock.o clntproc.o host.o svc.o svclock.o svcshare.o \
	    svcproc.o svcsubs.o mon.o xdr.o lockd_syms.o

obj-$(CONFIG_LOCKD_V4) += xdr4.o svc4proc.o

obj-m    := $(O_TARGET)

include $(TOPDIR)/Rules.make
