#	$Id: Makefile,v 1.4 1996/09/22 21:56:54 wosch Exp $

.PATH:	${.CURDIR}/../../sys/i386/isa
KMOD	= qcam_mod
SRCS	= qcam.c qcamio.c qcam.h
MAN8	= qcam.8

CFLAGS		+= -I. -DQCAM_MODULE
CLEANFILES	+= qcam.h

qcam.h:
	echo "#define NQCAM 1" > qcam.h

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	${.CURDIR}/qcam ${DESTDIR}/usr/bin

.include <bsd.kmod.mk>
