#	$Id: Makefile,v 1.7 1998/02/01 18:12:15 bde Exp $

.PATH:	${.CURDIR}/../../i386/isa
KMOD	= joy
SRCS	= joy.c joy.h opt_devfs.h
MAN8	= joy.8

CFLAGS		+= -DJOY_MODULE
CLEANFILES	+= joy.h opt_devfs.h

joy.h:
	echo "#define NJOY 1" > joy.h

opt_devfs.h:
	touch opt_devfs.h


afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	${.CURDIR}/joy ${DESTDIR}/usr/bin

.include <bsd.kmod.mk>
