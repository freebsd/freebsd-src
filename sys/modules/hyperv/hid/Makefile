.PATH:	${SRCTOP}/sys/dev/hyperv/input

KMOD=	hv_hid
SRCS=	hv_hid.c
SRCS+=	bus_if.h device_if.h hid_if.h vmbus_if.h

CFLAGS+= -I${SRCTOP}/sys/dev/hyperv/include \
	 -I${SRCTOP}/sys/dev/hyperv/vmbus

.include <bsd.kmod.mk>
