#
# Makefile for common code for Toshiba TX4927 based systems
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

O_TARGET:= tx4927.o

obj-y	:= tx4927_prom.o
obj-y	+= tx4927_setup.o
obj-y	+= tx4927_irq.o
obj-y	+= tx4927_irq_handler.o

obj-$(CONFIG_KGDB)                 += tx4927_dbgio.o

include $(TOPDIR)/Rules.make
