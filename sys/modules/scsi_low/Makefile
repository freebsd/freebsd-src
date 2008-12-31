# $FreeBSD: src/sys/modules/scsi_low/Makefile,v 1.2.32.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../cam/scsi
KMOD=	scsi_low
SRCS=	scsi_low.c scsi_low_pisa.c 
SRCS+=  opt_ddb.h opt_scsi.h opt_cam.h bus_if.h device_if.h

EXPORT_SYMS=	scsi_low_attach		\
		scsi_low_dettach	\
		scsi_low_is_busy	\
		scsi_low_activate	\
		scsi_low_deactivate	\
		scsi_low_bus_idle	\
		scsi_low_arbit_fail	\
		scsi_low_arbit_win	\
		scsi_low_msgout		\
		scsi_low_msgin		\
		scsi_low_statusin	\
		scsi_low_data		\
		scsi_low_data_finish	\
		scsi_low_cmd		\
		scsi_low_reselected	\
		scsi_low_disconnected	\
		scsi_low_restart	\
		scsi_low_print		\
		scsi_low_bus_reset

.include <bsd.kmod.mk>
