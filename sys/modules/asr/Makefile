# $FreeBSD: src/sys/modules/asr/Makefile,v 1.6.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/asr

KMOD=	asr
SRCS=	asr.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.if ${MACHINE_ARCH} == "i386"
SRCS+=	opt_asr.h
.endif

.include <bsd.kmod.mk>
