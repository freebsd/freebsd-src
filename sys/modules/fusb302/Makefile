.PATH:	${SRCTOP}/sys/dev/iicbus/usb

KMOD=	fusb302
SRCS=	fusb302.c
SRCS+=	device_if.h bus_if.h iicbus_if.h ofw_bus_if.h regnode_if.h
SRCS+=	opt_platform.h

.include <bsd.kmod.mk>
