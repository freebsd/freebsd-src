.PATH: ${.CURDIR}/../../dev/superio

KMOD=	superio
SRCS=	superio.c
SRCS+=	device_if.h bus_if.h isa_if.h

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
