#	$Id: Makefile,v 1.3 1996/09/22 21:56:49 wosch Exp $

.PATH:	${.CURDIR}/../../sys/i386/isa
KMOD	= joy_mod
SRCS	= joy.c joy.h
MAN8	= joy.8

CFLAGS		+= -I. -DJOY_MODULE
CLEANFILES	+= joy.h

joy.h:
	echo "#define NJOY 1" > joy.h


afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	${.CURDIR}/joy ${DESTDIR}/usr/bin

.include <bsd.kmod.mk>
