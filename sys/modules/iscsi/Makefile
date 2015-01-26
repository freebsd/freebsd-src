# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/iscsi/
KMOD=	iscsi

SRCS=	iscsi.c
SRCS+=	icl.c
SRCS+=	icl_proxy.c
SRCS+=	opt_cam.h
SRCS+=	bus_if.h
SRCS+=	device_if.h

#CFLAGS+=-DICL_KERNEL_PROXY

.include <bsd.kmod.mk>
