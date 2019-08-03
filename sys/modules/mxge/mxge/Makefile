#$FreeBSD$

.PATH: ${SRCTOP}/sys/dev/mxge

KMOD=	if_mxge
SRCS=	if_mxge.c device_if.h bus_if.h pci_if.h opt_inet.h opt_inet6.h

CFLAGS.if_mxge.c+= ${ZLIB_CFLAGS}

.include <bsd.kmod.mk>
