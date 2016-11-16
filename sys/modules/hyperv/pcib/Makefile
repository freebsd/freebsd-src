# $FreeBSD$

.PATH:  ${.CURDIR}/../../../dev/hyperv/pcib \
	${.CURDIR}/../../../dev/hyperv/vmbus

KMOD=	vmbus_pcib
SRCS=	pcib.c
SRCS+=	bus_if.h device_if.h vmbus_if.h

CFLAGS+= -I${.CURDIR}/../../../dev/hyperv/pcib

.include <bsd.kmod.mk>
