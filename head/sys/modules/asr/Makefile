# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/asr

KMOD=	asr
SRCS=	asr.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	opt_asr.h
.endif

.include <bsd.kmod.mk>
