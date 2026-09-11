.PATH: ${SRCTOP}/sys/dev/hid

KMOD=	ps5dsense
SRCS=	ps5dsense.c
SRCS+=	opt_hid.h opt_kbd.h
SRCS+=	bus_if.h device_if.h usbdevs.h

.include <bsd.kmod.mk>
