#	@(#)Makefile	8.1 (Berkeley) 6/4/93

LIB=	scsi
#CFLAGS+=-DLIBC_SCCS -I${.CURDIR}/../../sys
SRCS=	scsi.c

MAN3=	scsi.3

MLINKS+=scsi.3 scsireq_buff_decode.3 scsi.3 scsireq_build.3 \
	scsi.3 scsireq_decode.3 scsi.3 scsireq_encode.3 \
	scsi.3 scsireq_enter.3 scsi.3 scsireq_new.3 \
	scsi.3 scsireq_reset.3 scsi.3 SCSIREQ_ERROR.3 \
	scsi.3 scsi_open.3 scsi.3 scsi_debug.3 \
	scsi.3 scsi_debug_output.3

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/scsi.h \
	    ${DESTDIR}/usr/include


.include <bsd.lib.mk>
