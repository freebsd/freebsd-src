.PATH:	${.CURDIR}/../../sys/i386/isa
KMOD	= qcam_mod
SRCS	= qcam.c qcamio.c qcam.h
MAN8	= qcam.8

CFLAGS		+= -I. -I/sys -DQCAM_MODULE -DACTUALLY_LKM_NOT_KERNEL
CLEANFILES	+= qcam.h

qcam.h:
	echo "#define NQCAM 1" > qcam.h

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	${.CURDIR}/qcam ${DESTDIR}/usr/bin

.include <bsd.kmod.mk>
