# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/hyperv/utilities

KMOD=	hv_utils
SRCS=	hv_util.c
SRCS+=	bus_if.h device_if.h

CFLAGS+= -I${.CURDIR}/../../../dev/hyperv/include \
	 -I${.CURDIR}/../../../dev/hyperv/vmbus

.include <bsd.kmod.mk>
