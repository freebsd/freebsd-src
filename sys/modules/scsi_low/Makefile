# $FreeBSD$

.PATH: ${.CURDIR}/../../cam/scsi
KMOD=	scsi_low
SRCS=	scsi_low.c scsi_low_pisa.c 
SRCS+=  opt_ddb.h opt_scsi.h opt_cam.h bus_if.h device_if.h

.include <bsd.kmod.mk>
