
LIB=	cam
SRCS=	camlib.c scsi_cmdparse.c scsi_all.c scsi_da.c scsi_sa.c cam.c

# MAN3=	cam.3

# XXX KDM Need to write the man pages.
# MLINKS+= cam.3 cam_open_device.3 \
#	cam.3 cam_close_device.3 \
#	cam.3 cam_getccb.3 \
#	cam.3 cam_freeccb.3 \
#	cam.3 cam_send_ccb.3 \
#	cam.3 cam_device_dup.3 \
#	cam.3 cam_device_copy.3 \
#	cam.3 scsi_read_write.3 \
#	cam.3 scsi_start_stop.3 \
#	cam.3 scsi_sense_desc.3 \
#	cam.3 scsi_op_desc.3 \
#	cam.3 scsi_cdb_string.3 \
#	cam.3 scsi_print_inquriy.3 \
#	cam.3 scsi_calc_syncsrate.3 \   # XXX
#	cam.3 scsi_test_unit_ready.3 \
#	cam.3 scsi_inquiry.3 \
#	cam.3 scsi_read_capacity.3 \
#	cam.3 scsi_prevent.3 \
#	cam.3 scsi_synchronize_cache.3 \
#	cam.3 scsi_sense_string.3 \
#	cam.3 scsi_sense_print.3 \
#	cam.3 scsi_inqurity_match.3 \   # XXX
#	cam.3 scsi_extract_sense.3 \
#	cam.3 scsi_ulto2b.3 \
#	cam.3 scsi_ulto3b.3 \
#	cam.3 scsi_ulto4b.3 \
#	cam.3 scsi_2btoul.3 \
#	cam.3 scsi_3btoul.3 \
#	cam.3 scsi_3btol.3 \
#	cam.3 scsi_4btoul.3 

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/camlib.h \
	    ${DESTDIR}/usr/include

.PATH: ${.CURDIR}/../../sys/cam/scsi ${.CURDIR}/../../sys/cam

CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../sys

.include <bsd.lib.mk>
