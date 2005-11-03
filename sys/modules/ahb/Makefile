# $FreeBSD: src/sys/modules/ahb/Makefile,v 1.1 2004/06/29 20:50:24 imp Exp $

.PATH: ${.CURDIR}/../../dev/ahb

KMOD=	ahb
SRCS=	ahb.c opt_cam.h device_if.h bus_if.h \
	eisa_if.h opt_scsi.h

opt_scsi.h:
	echo "#define SCSI_DELAY 15000" > opt_scsi.h

.include <bsd.kmod.mk>
