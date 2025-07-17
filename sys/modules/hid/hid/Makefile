.PATH: ${SRCTOP}/sys/dev/hid

KMOD=	hid
SRCS=	hid.c hid_if.c
SRCS+=	opt_hid.h
SRCS+=	bus_if.h device_if.h hid_if.h

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
