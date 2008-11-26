# $FreeBSD: src/sys/modules/asr/Makefile,v 1.5.12.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/asr

KMOD=	asr
SRCS=	asr.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.if ${MACHINE_ARCH} == "alpha"
CFLAGS+= -O0
.endif

.if ${MACHINE_ARCH} == "i386"
SRCS+=	opt_asr.h
.endif

.include <bsd.kmod.mk>
