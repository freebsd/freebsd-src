# $FreeBSD$

.PATH:	${.CURDIR}/../../i386/isa
KMOD	= if_wi
SRCS	= if_wi.c device_if.h bus_if.h
CFLAGS	+= ${DEBUG_FLAGS}

.include <bsd.kmod.mk>
