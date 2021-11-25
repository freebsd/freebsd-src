# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/flash/flexspi

KMOD=	flexspi
SRCS=	flex_spi.c

# Generated files...
SRCS+=	\
	bus_if.h \
	device_if.h \
	ofw_bus_if.h \
	opt_platform.h \

.include <bsd.kmod.mk>
