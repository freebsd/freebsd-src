#
# Makefile for the Linux Bluetooth subsystem
#

O_TARGET    := bluetooth.o

list-multi  := bluez.o
export-objs := syms.o l2cap.o

bluez-objs  := af_bluetooth.o hci_core.o hci_conn.o hci_event.o hci_sock.o lib.o syms.o

obj-$(CONFIG_BLUEZ) += bluez.o
obj-$(CONFIG_BLUEZ_L2CAP) += l2cap.o
obj-$(CONFIG_BLUEZ_SCO) += sco.o

subdir-$(CONFIG_BLUEZ_RFCOMM) += rfcomm
subdir-$(CONFIG_BLUEZ_BNEP) += bnep
subdir-$(CONFIG_BLUEZ_CMTP) += cmtp

ifeq ($(CONFIG_BLUEZ_RFCOMM),y)
obj-y += rfcomm/rfcomm.o
endif

ifeq ($(CONFIG_BLUEZ_BNEP),y)
obj-y += bnep/bnep.o
endif

include $(TOPDIR)/Rules.make

bluez.o: $(bluez-objs)
	$(LD) -r -o $@ $(bluez-objs)
