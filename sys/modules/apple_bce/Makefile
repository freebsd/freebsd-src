.PATH: ${SRCTOP}/sys/dev/apple_bce

KMOD=	apple_bce
SRCS=	apple_bce.c apple_bce_mailbox.c apple_bce_queue.c
SRCS+=	apple_bce_vhci.c
SRCS+=	bus_if.h device_if.h pci_if.h
SRCS+=	opt_usb.h opt_bus.h

.include <bsd.kmod.mk>
