# $FreeBSD: src/sys/modules/asr/Makefile,v 1.6.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/asr

KMOD=	asr
SRCS=	asr.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.if ${MACHINE_ARCH} == "i386"
SRCS+=	opt_asr.h
.endif

.include <bsd.kmod.mk>
