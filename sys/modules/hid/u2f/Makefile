.PATH: ${SRCTOP}/sys/dev/hid

KMOD=	u2f
SRCS=	u2f.c
SRCS+=	opt_hid.h opt_usb.h
SRCS+=	bus_if.h device_if.h

.include <bsd.kmod.mk>
