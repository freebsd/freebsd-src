#
# Makefile for the kernel USB device drivers.
#

# Subdirs.

# The target object and module list name.

O_TARGET	:= usbdrv.o

# Objects that export symbols.

export-objs		:= hcd.o usb.o ov511.o pwc-uncompress.o

# Multipart objects.

list-multi		:= usbcore.o hid.o pwc.o
usbcore-objs		:= usb.o usb-debug.o hub.o
hid-objs		:= hid-core.o
pwc-objs		:= pwc-if.o pwc-misc.o pwc-ctrl.o pwc-uncompress.o
auerswald-objs		:= auerbuf.o auerchain.o auerchar.o auermain.o

# Optional parts of multipart objects.

ifeq ($(CONFIG_USB_DEVICEFS),y)
	usbcore-objs	+= devio.o inode.o drivers.o devices.o
endif

ifeq ($(CONFIG_USB_HIDDEV),y)
	hid-objs	+= hiddev.o
endif

ifeq ($(CONFIG_USB_HIDINPUT),y)
	hid-objs	+= hid-input.o
endif

ifdef CONFIG_USB_AUERISDN
ifneq ($(CONFIG_USB_AUERISDN),n)
	auerswald-objs	+= auerisdn.o auerisdn_b.o
endif
endif

# Object file lists.

obj-y	:=
obj-m	:=
obj-n	:=
obj-	:=

# Each configuration option enables a list of files.

obj-$(CONFIG_USB)		+= usbcore.o

# EHCI should initialize/link before the other HCDs
ifeq ($(CONFIG_USB_EHCI_HCD),y)
	obj-y += host/ehci-hcd.o
endif

ifneq ($(CONFIG_USB_EHCI_HCD),n)
	usbcore-objs		+= hcd.o
endif

subdir-$(CONFIG_USB_EHCI_HCD)	+= host

subdir-$(CONFIG_USB_UHCI_ALT)	+= host
ifeq ($(CONFIG_USB_UHCI_ALT),y)
	obj-y += host/uhci.o
endif

subdir-$(CONFIG_USB_UHCI)	+= host
ifeq ($(CONFIG_USB_UHCI),y)
	obj-y += host/usb-uhci.o
endif

subdir-$(CONFIG_USB_OHCI)	+= host
ifeq ($(CONFIG_USB_OHCI),y)
	obj-y += host/usb-ohci.o
endif

subdir-$(CONFIG_USB_SL811HS_ALT)	+= host
subdir-$(CONFIG_USB_SL811HS)	+= host

obj-$(CONFIG_USB_MOUSE)		+= usbmouse.o
obj-$(CONFIG_USB_HID)		+= hid.o
obj-$(CONFIG_USB_KBD)		+= usbkbd.o
obj-$(CONFIG_USB_AIPTEK)	+= aiptek.o
obj-$(CONFIG_USB_WACOM)		+= wacom.o
obj-$(CONFIG_USB_KBTAB)		+= kbtab.o
obj-$(CONFIG_USB_POWERMATE)	+= powermate.o

obj-$(CONFIG_USB_SCANNER)	+= scanner.o
obj-$(CONFIG_USB_ACM)		+= acm.o
obj-$(CONFIG_USB_PRINTER)	+= printer.o
obj-$(CONFIG_USB_AUDIO)		+= audio.o
obj-$(CONFIG_USB_EMI26)		+= emi26.o
obj-$(CONFIG_USB_MIDI)		+= usb-midi.o
obj-$(CONFIG_USB_IBMCAM)	+= ibmcam.o usbvideo.o ultracam.o
obj-$(CONFIG_USB_KONICAWC)	+= konicawc.o usbvideo.o
obj-$(CONFIG_USB_PWC)		+= pwc.o
obj-$(CONFIG_USB_DC2XX)		+= dc2xx.o
obj-$(CONFIG_USB_MDC800)	+= mdc800.o
obj-$(CONFIG_USB_USS720)	+= uss720.o
obj-$(CONFIG_USB_DABUSB)	+= dabusb.o
obj-$(CONFIG_USB_VICAM)		+= vicam.o usbvideo.o
obj-$(CONFIG_USB_OV511)		+= ov511.o
obj-$(CONFIG_USB_W9968CF)	+= w9968cf.o
obj-$(CONFIG_USB_SE401)		+= se401.o
obj-$(CONFIG_USB_STV680)	+= stv680.o
obj-$(CONFIG_USB_PEGASUS)	+= pegasus.o
obj-$(CONFIG_USB_RTL8150)	+= rtl8150.o
obj-$(CONFIG_USB_CATC)		+= catc.o
obj-$(CONFIG_USB_KAWETH)        += kaweth.o
obj-$(CONFIG_USB_CDCETHER)	+= CDCEther.o
obj-$(CONFIG_USB_RIO500)	+= rio500.o
obj-$(CONFIG_USB_TIGL)          += tiglusb.o
obj-$(CONFIG_USB_DSBR)		+= dsbr100.o
obj-$(CONFIG_USB_MICROTEK)	+= microtek.o
obj-$(CONFIG_USB_HPUSBSCSI)	+= hpusbscsi.o
obj-$(CONFIG_USB_BLUETOOTH)	+= bluetooth.o
obj-$(CONFIG_USB_USBNET)	+= usbnet.o
obj-$(CONFIG_USB_AUERSWALD)	+= auerswald.o
obj-$(CONFIG_USB_BRLVGER)	+= brlvger.o
obj-$(CONFIG_USB_LCD)		+= usblcd.o
obj-$(CONFIG_USB_SPEEDTOUCH)	+= speedtch.o

# Object files in subdirectories
mod-subdirs	:= serial host

subdir-$(CONFIG_USB_SERIAL)	+= serial
subdir-$(CONFIG_USB_STORAGE)	+= storage

ifeq ($(CONFIG_USB_SERIAL),y)
	obj-y += serial/usb-serial.o
endif

ifeq ($(CONFIG_USB_STORAGE),y)
	obj-y += storage/storage.o
endif

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

usbcore.o: $(usbcore-objs)
	$(LD) -r -o $@ $(usbcore-objs)

hid.o: $(hid-objs)
	$(LD) -r -o $@ $(hid-objs)

pwc.o: $(pwc-objs)
	$(LD) -r -o $@ $(pwc-objs)

auerswald.o: $(auerswald-objs)
	$(LD) -r -o $@ $(auerswald-objs)
