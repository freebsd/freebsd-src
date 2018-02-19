# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/iicbus

KMOD=	ofw_iicbus
SRCS=	ofw_iicbus.c

SRCS+=	\
	bus_if.h \
	device_if.h \
	iicbus_if.h \
	ofw_bus_if.h \
	opt_platform.h \

.include <bsd.kmod.mk>
