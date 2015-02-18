# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/iscsi/
KMOD=	iscsi

SRCS=	iscsi.c
SRCS+=	icl.c
SRCS+=	icl_proxy.c
SRCS+=	icl_soft.c
SRCS+=	opt_cam.h
SRCS+=	bus_if.h
SRCS+=	device_if.h
SRCS+=	icl_conn_if.c
SRCS+=	icl_conn_if.h

#CFLAGS+=-DICL_KERNEL_PROXY

MFILES=	kern/bus_if.m kern/device_if.m dev/iscsi/icl_conn_if.m

.include <bsd.kmod.mk>
