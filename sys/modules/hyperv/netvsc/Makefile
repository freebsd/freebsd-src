# $FreeBSD$

.PATH:  ${.CURDIR}/../../../dev/hyperv/netvsc \
	${.CURDIR}/../../../dev/hyperv/vmbus

KMOD=	hv_netvsc
SRCS=	hn_nvs.c hn_rndis.c hv_netvsc_drv_freebsd.c
SRCS+=	bus_if.h device_if.h opt_inet.h opt_inet6.h vmbus_if.h

CFLAGS+= -I${.CURDIR}/../../../dev/hyperv/netvsc

.include <bsd.kmod.mk>
