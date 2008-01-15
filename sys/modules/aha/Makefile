# $FreeBSD: src/sys/modules/aha/Makefile,v 1.8.14.1 2006/02/10 00:37:58 yar Exp $

.PATH: ${.CURDIR}/../../dev/aha

KMOD=	aha
SRCS=	aha.c aha_isa.c ahareg.h opt_cam.h device_if.h bus_if.h \
	opt_scsi.h isa_if.h

.if !defined(KERNBUILDDIR)
opt_scsi.h:
	echo "#define SCSI_DELAY 15000" > opt_scsi.h
.endif

.include <bsd.kmod.mk>
