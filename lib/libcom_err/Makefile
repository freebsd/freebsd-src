#	$Id: Makefile,v 1.2 1995/08/06 12:23:28 bde Exp $

LIB=	com_err
SRCS=	com_err.c error_message.c et_name.c init_et.c
CFLAGS+=	-I.
MAN3=	com_err.3

SUBDIR=	doc

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/com_err.h \
	    ${DESTDIR}/usr/include

.include <bsd.lib.mk>

