#
# Makefile for the DECstation prom monitor library routines
# under Linux.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

USE_STANDARD_AS_RULE := true

L_TARGET  = rexlib.a

obj-y			+= init.o memory.o cmdline.o identify.o

obj-$(CONFIG_MIPS32)	+= locore.o
obj-$(CONFIG_MIPS64)	+= call_o32.o

include $(TOPDIR)/Rules.make
