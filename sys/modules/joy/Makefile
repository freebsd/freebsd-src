# $FreeBSD$

.PATH:	${.CURDIR}/../../i386/isa
KMOD	= joy
SRCS	= joy.c opt_devfs.h
MAN8	= joy.8

CFLAGS		+= -DJOY_MODULE
CLEANFILES	+= opt_devfs.h

opt_devfs.h:
	touch opt_devfs.h


afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/joy.sh ${DESTDIR}/usr/bin/joy

.include <bsd.kmod.mk>
