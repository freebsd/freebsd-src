# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/mfi

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
SUBDIR= mfi_linux
.endif

KMOD=	mfi
SRCS=	mfi.c mfi_pci.c mfi_disk.c
SRCS+=	opt_mfi.h
SRCS+=	device_if.h bus_if.h pci_if.h
CFLAGS+= -Wall -Werror

# To enable debug output from the driver, uncomment these two lines.
#CFLAGS+= -DMFI_DEBUG=2
#SRCS+=	mfi_debug.c

.include <bsd.kmod.mk>
