# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/hyperv/vmbus \
	${.CURDIR}/../../../dev/hyperv/utilities

KMOD=	hv_vmbus
SRCS=	hv_channel.c \
	hv_channel_mgmt.c \
	hv_connection.c \
	hv_et.c \
	hv_hv.c \
	hv_ring_buffer.c \
	hv_vmbus_drv_freebsd.c \
	hv_vmbus_priv.h
SRCS+=	acpi_if.h bus_if.h device_if.h opt_acpi.h

CFLAGS+= -I${.CURDIR}/../../../dev/hyperv/include \
	 -I${.CURDIR}/../../../dev/hyperv/vmbus \
	 -I${.CURDIR}/../../../dev/hyperv/utilities

.include <bsd.kmod.mk>
