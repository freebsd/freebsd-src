# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/mcd

KMOD=	mcd
SRCS=	mcd.c mcd_isa.c
SRCS+=	bus_if.h device_if.h isa_if.h opt_geom.h

opt_geom.h:
	@echo > opt_geom.h

.include <bsd.kmod.mk>
