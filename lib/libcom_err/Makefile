#	$Id: Makefile,v 1.4 1996/09/04 22:22:46 bde Exp $

LIB=	com_err
SRCS=	com_err.c error_message.c et_name.c init_et.c
MAN3=	com_err.3

#SUBDIR=	doc

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/com_err.h \
	    ${DESTDIR}/usr/include

.include <bsd.lib.mk>

