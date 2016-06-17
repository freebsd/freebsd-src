#
# linux/arch/arm/lib/Makefile
#
# Copyright (C) 1995-2000 Russell King
#

USE_STANDARD_AS_RULE := true

L_TARGET	:= lib.a

obj-y		:= backtrace.o changebit.o csumipv6.o csumpartial.o   \
		   csumpartialcopy.o csumpartialcopyuser.o clearbit.o \
		   copy_page.o delay.o findbit.o memchr.o memcpy.o    \
		   memset.o memzero.o setbit.o strncpy_from_user.o    \
		   strnlen_user.o strchr.o strrchr.o testchangebit.o  \
		   testclearbit.o testsetbit.o uaccess.o getuser.o    \
		   putuser.o ashldi3.o ashrdi3.o lshrdi3.o muldi3.o   \
		   ucmpdi2.o udivdi3.o lib1funcs.o
obj-m		:=
obj-n		:=

obj-$(CONFIG_VT)+= kbd.o

obj-arc		:= ecard.o io-acorn.o floppydma.o
obj-rpc		:= ecard.o io-acorn.o floppydma.o
obj-clps7500	:= io-acorn.o
obj-l7200     	:= io-acorn.o
obj-shark	:= io-shark.o
obj-edb7211	:= io-acorn.o
obj-riscstation := io-acorn.o floppydma.o

obj-y		+= $(obj-$(MACHINE))

ifeq ($(CONFIG_CPU_32v3),y)
  v3		:= y
  v4		:= n
else
  v3		:= n
  v4		:= y
endif

obj-y		+= io-readsb.o io-writesb.o
obj-$(v3)	+= io-readsw-armv3.o io-writesw-armv3.o io-readsl-armv3.o
obj-$(v4)	+= io-readsw-armv4.o io-writesw-armv4.o io-readsl-armv4.o
obj-y		+= io-writesl.o

ifeq ($(PROCESSOR),armo)
  obj-y		+= uaccess-armo.o
endif

include $(TOPDIR)/Rules.make

csumpartialcopy.o: csumpartialcopygeneric.S
csumpartialcopyuser.o: csumpartialcopygeneric.S
