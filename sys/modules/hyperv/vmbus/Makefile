# $FreeBSD$

.PATH:	${.CURDIR}/../../../contrib/dev/hyperv/vmbus \
	${.CURDIR}/../../../contrib/dev/hyperv/utilities

KMOD=	 hv_vmbus

SRCS =  hv_channel.c \
	hv_channel_mgmt.c \
	hv_connection.c \
	hv_hv.c \
	hv_ring_buffer.c \
	hv_vmbus_drv_freebsd.c \
	hv_vmbus_priv.h

CFLAGS+= -I${.CURDIR}/../../../contrib/dev/hyperv/include \
	 -I${.CURDIR}/../../../contrib/dev/hyperv/vmbus \
	 -I${.CURDIR}/../../../contrib/dev/hyperv/utilities \
	 -I${.CURDIR}/../../../contrib

.include <bsd.kmod.mk>
