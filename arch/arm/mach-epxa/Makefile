#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= epxa.o

# Object file lists.

obj-y			:=  irq.o mm.o time.o
obj-m			:=
obj-n			:=
obj-			:=

export-objs		:= 

obj-$(CONFIG_EPXA1DB) += arch-epxa1db.o
obj-$(CONFIG_EPXA10DB) += arch-epxa10db.o

include $(TOPDIR)/Rules.make
