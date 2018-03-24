# $FreeBSD$

.PATH:		${SRCTOP}/sys/dev/iicbus
KMOD		= iicbus
SRCS		= device_if.h bus_if.h iicbus_if.h iicbus_if.c \
		  iiconf.h iiconf.c iicbus.h iicbus.c

.if !empty(OPT_FDT)
SRCS+=	ofw_iicbus.c ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
