# $FreeBSD: src/sys/modules/asr/Makefile,v 1.3 2001/04/01 08:33:01 scottl Exp $

.PATH: ${.CURDIR}/../../dev/asr

KMOD=	asr
SRCS=	asr.c
SRCS+=	opt_scsi.h opt_cam.h opt_asr.h
SRCS+=	device_if.h bus_if.h pci_if.h

.if ${MACHINE_ARCH} == "alpha"
CFLAGS+= -O0
.endif

.include <bsd.kmod.mk>
