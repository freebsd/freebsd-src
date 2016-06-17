#
# Makefile for the IP27 specific kernel interface routines under Linux.
#

USE_STANDARD_AS_RULE := true

O_TARGET = ip27.o

obj-y	:= ip27-berr.o ip27-console.o ip27-irq.o ip27-init.o ip27-irq-glue.o \
	   ip27-klconfig.o ip27-klnuma.o ip27-memory.o ip27-nmi.o ip27-pci.o \
	   ip27-reset.o ip27-setup.o ip27-timer.o

include $(TOPDIR)/Rules.make
