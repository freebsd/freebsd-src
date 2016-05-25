# $FreeBSD$

.PATH: ${.CURDIR}/../../../../dev/bhnd/cores/chipc

KMOD=	bhnd_chipc
SRCS=	chipc.c chipc_subr.c \
	bhnd_sprom_chipc.c
SRCS+=	device_if.h bus_if.h bhnd_bus_if.h \
	bhnd_chipc_if.h bhnd_nvram_if.h

.include <bsd.kmod.mk>
