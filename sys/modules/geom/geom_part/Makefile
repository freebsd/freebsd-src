# $FreeBSD$

.PATH: ${.CURDIR}/../../../geom/part

KMOD=	geom_part
SRCS=	g_part.c \
	g_part_if.c g_part_if.h \
	g_part_apm.c \
	g_part_bsd.c \
	g_part_gpt.c \
	g_part_mbr.c \
	g_part_vtoc8.c

SRCS+=	bus_if.h device_if.h

MFILES=	kern/bus_if.m kern/device_if.m geom/part/g_part_if.m

.include <bsd.kmod.mk>
