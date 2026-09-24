# $FreeBSD$

.PATH:  ${SRCTOP}/sys/dev/hyperv/hvuio \
	${SRCTOP}/sys/dev/hyperv/vmbus

KMOD=	hv_uio
SRCS=	hv_uio.c
SRCS+=	device_if.h bus_if.h vmbus_if.h

CFLAGS+= -I${SRCTOP}/sys/dev/hyperv/hvuio

.include <bsd.kmod.mk>
