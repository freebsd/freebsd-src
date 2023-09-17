
.PATH: ${SRCTOP}/sys/dev/hid

KMOD=	hconf
SRCS=	hconf.c
SRCS+=	opt_hid.h
SRCS+=	bus_if.h device_if.h

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
