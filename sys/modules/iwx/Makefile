.PATH:  ${SRCTOP}/sys/dev/iwx

KMOD=	if_iwx
SRCS=	if_iwx.c if_iwx_debug.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_wlan.h

CFLAGS+= -DIWX_DEBUG=1
# DEBUG_FLAGS=	-O0 -g

.include <bsd.kmod.mk>
