# $FreeBSD$

.PATH: ${SRCTOP}/sys/compat/lindebugfs

KMOD=	lindebugfs
SRCS=	vnode_if.h \
	device_if.h bus_if.h  pci_if.h \
	lindebugfs.c

CFLAGS+= -I${SRCTOP}/sys/compat/linuxkpi/common/include

.include <bsd.kmod.mk>
