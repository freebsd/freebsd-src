# $FreeBSD$

.PATH: ${SRCTOP}/sys/arm/allwinner

KMOD=	aw_thermal
SRCS=	aw_thermal.c

SRCS+=	\
	bus_if.h \
	clknode_if.h \
	device_if.h \
	ofw_bus_if.h \

.include <bsd.kmod.mk>
