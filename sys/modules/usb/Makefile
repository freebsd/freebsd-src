#       $FreeBSD$ 

MAINTAINER	 = n_hibma@freebsd.org
S		 = ${.CURDIR}/../..

.PATH:		 $S/dev/usb $S/pci
KMOD		 = usb
SRCS		 = bus_if.h device_if.h usb_if.h usb_if.c \
		   vnode_if.h \
		   opt_usb.h \
		   hid.c hid.h usbhid.h \
		   uhub.c \
		   usb.c usb.h \
		   usb_mem.h \
		   usb_port.h \
		   usb_quirks.c usb_quirks.h \
		   usb_subr.c \
		   usbdevs.h usbdevs_data.h \
		   usbdi.c usbdi.h usbdivar.h \
		   usbdi_util.c usbdi_util.h

SRCS		+= uhci_pci.c uhci.c uhcireg.h uhcivar.h
SRCS		+= ohci_pci.c ohci.c ohcireg.h ohcivar.h
SRCS		+= opt_bus.h pci_if.h

NOMAN		 =
CFLAGS		+= -g

opt_usb.h:
	echo > opt_usb.h
#	echo "#define USB_DEBUG 1" >> opt_usb.h
#	echo "#define UHCI_DEBUG 1" >> opt_usb.h
#	echo "#define OHCI_DEBUG 1" >> opt_usb.h

usb_devs.h:
	perl $S/kern/makedevlist.pl -h $S/dev/usb/usb.devs

.include <bsd.kmod.mk>
