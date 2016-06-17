#
# Makefile for drivers/acorn/scsi
#

USE_STANDARD_AS_RULE := true

O_TARGET	:= acorn-scsi.o

obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

export-objs		:= fas216.o queue.o msgqueue.o
list-multi		:= acornscsi_mod.o
acornscsi_mod-objs	:= acornscsi.o acornscsi-io.o

obj-$(CONFIG_SCSI_ACORNSCSI_3)	+= acornscsi_mod.o queue.o msgqueue.o
obj-$(CONFIG_SCSI_ARXESCSI)	+= arxescsi.o fas216.o queue.o msgqueue.o
obj-$(CONFIG_SCSI_CUMANA_1)	+= cumana_1.o
obj-$(CONFIG_SCSI_CUMANA_2)	+= cumana_2.o fas216.o queue.o msgqueue.o
obj-$(CONFIG_SCSI_ECOSCSI)	+= ecoscsi.o
obj-$(CONFIG_SCSI_OAK1)		+= oak.o
obj-$(CONFIG_SCSI_POWERTECSCSI)	+= powertec.o fas216.o queue.o msgqueue.o
obj-$(CONFIG_SCSI_EESOXSCSI)	+= eesox.o fas216.o queue.o msgqueue.o

include $(TOPDIR)/Rules.make

acornscsi_mod.o: $(acornscsi_mod-objs)
	$(LD) -r -o $@ $(acornscsi_mod-objs)
