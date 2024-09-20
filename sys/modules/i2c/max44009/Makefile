.PATH:	${SRCTOP}/sys/dev/iicbus/sensor
KMOD=	max44009
SRCS=	max44009.c

SRCS+=	\
	bus_if.h \
	device_if.h \
	iicbus_if.h \
	opt_platform.h

.if !empty(OPT_FDT)
SRCS+=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
