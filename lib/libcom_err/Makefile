#	$Id$

LIB=	com_err
SRCS=	com_err.c error_message.c et_name.c init_et.c
CFLAGS+=	-I.
MAN3=	com_err.3

SUBDIR=	doc

beforeinstall:
	-cd ${.CURDIR}; cmp -s com_err.h ${DESTDIR}/usr/include/com_err.h || \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 com_err.h \
	    ${DESTDIR}/usr/include

.include <bsd.lib.mk>

