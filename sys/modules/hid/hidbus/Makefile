.PATH: ${SRCTOP}/sys/dev/hid

KMOD=	hidbus
SRCS=	hidbus.c
SRCS+=	bus_if.h device_if.h hid_if.h

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
