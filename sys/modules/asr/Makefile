# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/asr

KMOD=	asr
SRCS=	asr.c
SRCS+=	opt_scsi.h opt_cam.h opt_asr.h
SRCS+=	device_if.h bus_if.h pci_if.h

.if ${MACHINE_ARCH} == "alpha"
CFLAGS+= -O0
.endif

.include <bsd.kmod.mk>
