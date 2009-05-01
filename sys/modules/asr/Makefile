# $FreeBSD: src/sys/modules/asr/Makefile,v 1.6.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/asr

KMOD=	asr
SRCS=	asr.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.if ${MACHINE_ARCH} == "i386"
SRCS+=	opt_asr.h
.endif

.include <bsd.kmod.mk>
