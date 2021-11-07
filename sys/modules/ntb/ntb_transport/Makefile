# $FreeBSD$

.PATH:  ${SRCTOP}/sys/dev/ntb

KMOD    = ntb_transport
SRCS    = ntb_transport.c
SRCS += device_if.h bus_if.h ntb_if.h

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
