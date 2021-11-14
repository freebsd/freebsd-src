# $FreeBSD$

.PATH: ${SRCTOP}/sys/arm/allwinner

KMOD=	aw_mmc
SRCS=	aw_mmc.c

SRCS+=	\
	bus_if.h \
	clknode_if.h \
	device_if.h \
	mmc_pwrseq_if.h \
	ofw_bus_if.h

.include <bsd.kmod.mk>
