# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/etherswitch/mtkswitch

KMOD=   mtkswitch
SRCS=   mtkswitch.c mtkswitch_mt7531.c 

SRCS+=  bus_if.h etherswitch_if.h mdio_if.h miibus_if.h ofw_bus_if.h

.include <bsd.kmod.mk>

