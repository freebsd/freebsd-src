# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/aac

KMOD=	aac
SRCS=	aac.c aac_pci.c aac_disk.c opt_aac.h
SRCS+=	device_if.h bus_if.h pci_if.h

CFLAGS+= -DAAC_COMPAT_LINUX

# To enable debug output from the driver, uncomment these two lines.
#CFLAGS+= -DAAC_DEBUG=2
#SRCS+=	aac_debug.c

.include <bsd.kmod.mk>
