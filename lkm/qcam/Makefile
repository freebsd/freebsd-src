#	$Id: Makefile,v 1.7 1998/01/24 02:21:52 eivind Exp $

.PATH:	${.CURDIR}/../../sys/i386/isa
KMOD	= qcam_mod
SRCS	= qcam.c qcamio.c opt_devfs.h qcam.h
MAN8	= qcam.8

CFLAGS		+= -DQCAM_MODULE
CLEANFILES	+= opt_devfs.h qcam.h

opt_devfs.h:
	touch opt_devfs.h

qcam.h:
	echo "#define NQCAM 1" > qcam.h

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	${.CURDIR}/qcam ${DESTDIR}/usr/bin

.include <bsd.kmod.mk>
