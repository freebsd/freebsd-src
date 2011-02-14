# $FreeBSD$

.PATH: ${.CURDIR}/../../../geom/raid

KMOD=	geom_raid
SRCS=	g_raid.c
SRCS+=	g_raid_ctl.c
SRCS+=	bus_if.h device_if.h
SRCS+=	g_raid_md_if.h g_raid_md_if.c
SRCS+=	g_raid_tr_if.h g_raid_tr_if.c

SRCS+=	md_intel.c

SRCS+=	tr_raid0.c tr_raid1.c tr_raid1e.c

MFILES=	kern/bus_if.m kern/device_if.m
MFILES+= geom/raid/g_raid_md_if.m geom/raid/g_raid_tr_if.m

.include <bsd.kmod.mk>
