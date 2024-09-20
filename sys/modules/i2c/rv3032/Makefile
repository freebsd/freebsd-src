.PATH:	${SRCTOP}/sys/dev/iicbus/rtc
KMOD=	rv3032
SRCS=	rv3032.c

SRCS+=	bus_if.h \
	clock_if.h \
	device_if.h \
	iicbus_if.h \
	opt_platform.h \
	ofw_bus_if.h

.include <bsd.kmod.mk>
