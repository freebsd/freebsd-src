#
# Makefile for Linux kernel SUN RPC
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := sunrpc.o

export-objs := sunrpc_syms.o

obj-y    := clnt.o xprt.o sched.o \
	    auth.o auth_null.o auth_unix.o \
	    svc.o svcsock.o svcauth.o \
	    pmap_clnt.o timer.o xdr.o sunrpc_syms.o

obj-$(CONFIG_PROC_FS) += stats.o
obj-$(CONFIG_SYSCTL) += sysctl.o

obj-m  := $(O_TARGET)

include $(TOPDIR)/Rules.make
