# $FreeBSD: src/lib/libcam/Makefile,v 1.6.2.1 2000/07/01 23:35:31 ken Exp $

MAINTAINER=ken@FreeBSD.ORG

LIB=	cam
SRCS=	camlib.c scsi_cmdparse.c scsi_all.c scsi_da.c scsi_sa.c cam.c
INCS=	camlib.h

MAN3=	cam.3 cam_cdbparse.3


MLINKS+=cam.3 cam_open_device.3 \
	cam.3 cam_open_spec_device.3 \
	cam.3 cam_open_btl.3 \
	cam.3 cam_open_pass.3 \
	cam.3 cam_close_device.3 \
	cam.3 cam_close_spec_device.3 \
	cam.3 cam_getccb.3 \
	cam.3 cam_send_ccb.3 \
	cam.3 cam_freeccb.3 \
	cam.3 cam_path_string.3 \
	cam.3 cam_device_dup.3 \
	cam.3 cam_device_copy.3 \
	cam.3 cam_get_device.3 \
	cam_cdbparse.3 csio_build.3 \
	cam_cdbparse.3 csio_build_visit.3 \
	cam_cdbparse.3 csio_decode.3 \
	cam_cdbparse.3 csio_decode_visit.3 \
	cam_cdbparse.3 buff_decode.3 \
	cam_cdbparse.3 buff_decode_visit.3 \
	cam_cdbparse.3 csio_encode.3 \
	cam_cdbparse.3 csio_encode_visit.3 \
	cam_cdbparse.3 buff_encode_visit.3

.PATH: ${.CURDIR}/../../sys/cam/scsi ${.CURDIR}/../../sys/cam

SDIR= ${.CURDIR}/../../sys
CFLAGS+=-I${.CURDIR} -I${SDIR}

.include <bsd.lib.mk>
