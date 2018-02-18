# $FreeBSD$

.PATH:	${SRCTOP}/sys/dev/iicbus

KMOD=	iicbus
SRCS=	\
	bus_if.h \
	device_if.h \
	iic_recover_bus.c \
	iicbus.c \
	iicbus.h \
	iicbus_if.c \
	iicbus_if.h \
	iiconf.c \
	iiconf.h \

.include <bsd.kmod.mk>
