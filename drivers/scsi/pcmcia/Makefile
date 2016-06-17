#
# drivers/scsi/pcmcia/Makefile
#
# Makefile for the Linux PCMCIA SCSI drivers.
#

obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

vpath %c ..

CFLAGS_aha152x.o = -DPCMCIA -D__NO_VERSION__ -DAHA152X_STAT
CFLAGS_fdomain.o = -DPCMCIA -D__NO_VERSION__
CFLAGS_qlogicfas.o = -DPCMCIA -D__NO_VERSION__

# 16-bit client drivers
obj-$(CONFIG_PCMCIA_QLOGIC)	+= qlogic_cs.o
obj-$(CONFIG_PCMCIA_FDOMAIN)	+= fdomain_cs.o
obj-$(CONFIG_PCMCIA_AHA152X)	+= aha152x_cs.o
obj-$(CONFIG_PCMCIA_NINJA_SCSI)	+= nsp_cs.o

list-multi	:= qlogic_cs.o fdomain_cs.o aha152x_cs.o
aha152x_cs-objs	:= aha152x_stub.o aha152x.o
fdomain_cs-objs	:= fdomain_stub.o fdomain.o
qlogic_cs-objs	:= qlogic_stub.o qlogicfas.o

include $(TOPDIR)/Rules.make

aha152x_cs.o: $(aha152x_cs-objs)
	$(LD) -r -o $@ $(aha152x_cs-objs)

fdomain_cs.o: $(fdomain_cs-objs)
	$(LD) -r -o $@ $(fdomain_cs-objs)

qlogic_cs.o: $(qlogic_cs-objs)
	$(LD) -r -o $@ $(qlogic_cs-objs)
