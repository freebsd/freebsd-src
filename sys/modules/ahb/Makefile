# $FreeBSD: src/sys/modules/ahb/Makefile,v 1.2.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ahb

KMOD=	ahb
SRCS=	ahb.c opt_cam.h device_if.h bus_if.h \
	eisa_if.h opt_scsi.h

.if !defined(KERNBUILDDIR)
opt_scsi.h:
	echo "#define SCSI_DELAY 15000" > opt_scsi.h
.endif

.include <bsd.kmod.mk>
