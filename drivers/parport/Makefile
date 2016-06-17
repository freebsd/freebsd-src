#
# Makefile for the kernel Parallel port device drivers.
#
# Note! Parport is the Borg.  We have assimilated some other
# drivers in the `char', `net' and `scsi' directories,
# but left them there to allay suspicion.
#
# 7 October 2000, Bartlomiej Zolnierkiewicz <bkz@linux-ide.org>
# Rewritten to use lists instead of if-statements.
#

O_TARGET := driver.o

export-objs	:= init.o parport_pc.o

list-multi	:= parport.o
parport-objs	:= share.o ieee1284.o ieee1284_ops.o init.o procfs.o

ifeq ($(CONFIG_PARPORT_1284),y)
	parport-objs	+= daisy.o probe.o
endif

obj-$(CONFIG_PARPORT)		+= parport.o
obj-$(CONFIG_PARPORT_PC)	+= parport_pc.o
obj-$(CONFIG_PARPORT_SERIAL)	+= parport_serial.o
obj-$(CONFIG_PARPORT_PC_PCMCIA)	+= parport_cs.o
obj-$(CONFIG_PARPORT_AMIGA)	+= parport_amiga.o
obj-$(CONFIG_PARPORT_MFC3)	+= parport_mfc3.o
obj-$(CONFIG_PARPORT_ATARI)	+= parport_atari.o
obj-$(CONFIG_PARPORT_SUNBPP)	+= parport_sunbpp.o
obj-$(CONFIG_PARPORT_GSC)	+= parport_gsc.o
obj-$(CONFIG_PARPORT_IP22)	+= parport_ip22.o

include $(TOPDIR)/Rules.make

parport.o: $(parport-objs)
	$(LD) -r -o $@ $(parport-objs)
