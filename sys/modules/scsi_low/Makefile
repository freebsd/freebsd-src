# $FreeBSD: src/sys/modules/scsi_low/Makefile,v 1.1.2.1 2001/12/17 13:32:31 non Exp $

.PATH: ${.CURDIR}/../../cam/scsi
KMOD=	scsi_low
SRCS=	scsi_low.c scsi_low_pisa.c 
SRCS+=  opt_ddb.h opt_scsi.h opt_cam.h bus_if.h device_if.h

.include <bsd.kmod.mk>
