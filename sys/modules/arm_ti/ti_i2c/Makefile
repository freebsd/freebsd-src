.PATH: ${SRCTOP}/sys/arm/ti

KMOD=	ti_i2c
SRCS=	ti_i2c.c

SRCS+=	\
	bus_if.h \
	device_if.h \
	iicbus_if.h \
	ofw_bus_if.h \

.include <bsd.kmod.mk>
