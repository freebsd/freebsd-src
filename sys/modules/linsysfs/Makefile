# $FreeBSD$

.PATH: ${.CURDIR}/../../compat/linsysfs

KMOD=	linsysfs
SRCS=	vnode_if.h \
	device_if.h bus_if.h  pci_if.h \
	linsysfs.c \
	opt_compat.h

.if ${MACHINE_CPUARCH} == "amd64"
CFLAGS+=-DCOMPAT_LINUX32
.endif

.include <bsd.kmod.mk>
