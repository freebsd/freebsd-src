#
# Makefile for the SGI specific kernel interface routines
# under Linux.
#
# Note: Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET := ip22-kern.o

all: ip22-kern.o ip22-irq.o

obj-y	+= ip22-mc.o ip22-hpc.o ip22-int.o ip22-irq.o ip22-berr.o \
	   ip22-time.o ip22-rtc.o ip22-nvram.o ip22-reset.o \
	   ip22-setup.o ip22-ksyms.o

obj-$(CONFIG_EISA)	+= ip22-eisa.o

export-objs	:= ip22-ksyms.o

ip22-irq.o: ip22-irq.S

include $(TOPDIR)/Rules.make
