.PATH: ${SRCTOP}/sys/arm/allwinner

KMOD=	axp81x
SRCS=	axp81x.c

SRCS+=	\
	bus_if.h \
	clknode_if.h \
	device_if.h \
	ofw_bus_if.h \
	iicbus_if.h

.include <bsd.kmod.mk>
