#	$Id: Makefile,v 1.6 1998/01/24 02:21:51 eivind Exp $

.PATH:	${.CURDIR}/../../sys/i386/isa
KMOD	= joy_mod
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
