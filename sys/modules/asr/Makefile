# $FreeBSD: src/sys/modules/asr/Makefile,v 1.4.2.1 2005/03/24 17:05:45 cognet Exp $

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
