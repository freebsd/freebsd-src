# $FreeBSD$

.PATH: ${.CURDIR}/../../../../dev/bhnd/cores/chipc

KMOD=	bhnd_chipc
SRCS=	chipc.c	\
	bhnd_chipc_if.c bhnd_chipc_if.h
SRCS+=	device_if.h bus_if.h bhnd_bus_if.h

.include <bsd.kmod.mk>
