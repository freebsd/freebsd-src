#	$Id: Makefile,v 1.5 1997/01/12 05:03:28 peter Exp $

LIB=	com_err
SRCS=	com_err.c error_message.c et_name.c init_et.c
MAN3=	com_err.3

SUBDIR=	doc

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/com_err.h \
	    ${DESTDIR}/usr/include

.include <bsd.lib.mk>

