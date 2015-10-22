# $FreeBSD$
.PATH:	${.CURDIR}/../../ofed/include/linux

KMOD=	linuxkpi
SRCS=	linux_kmod.c \
	linux_compat.c \
	linux_pci.c \
	linux_radix.c \
	linux_idr.c

SRCS+=	bus_if.h \
	device_if.h \
	pci_if.h \
	vnode_if.h

CFLAGS+= -I${.CURDIR}/../../ofed/include/

.include <bsd.kmod.mk>
