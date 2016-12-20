# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/hyperv/utilities

KMOD=	hv_utils
SRCS=	vmbus_ic.c
SRCS+=	hv_heartbeat.c
SRCS+=	hv_kvp.c
SRCS+=	hv_shutdown.c
SRCS+=	hv_snapshot.c
SRCS+=	hv_timesync.c
SRCS+=	bus_if.h device_if.h vmbus_if.h

CFLAGS+= -I${.CURDIR}/../../../dev/hyperv/include \
	 -I${.CURDIR}/../../../dev/hyperv/vmbus

.include <bsd.kmod.mk>
