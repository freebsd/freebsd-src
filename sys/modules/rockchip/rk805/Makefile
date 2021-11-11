# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/iicbus/pmic/rockchip

KMOD=	rk805
SRCS=	rk805.c

SRCS+=	\
	bus_if.h \
	device_if.h \
	iicbus_if.h \
	regnode_if.h \
	regdev_if.h \
	ofw_bus_if.h \
	opt_platform.h \

.include <bsd.kmod.mk>
