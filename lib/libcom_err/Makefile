# $FreeBSD$

LIB=	com_err
SRCS=	com_err.c error.c
INCS=	${COM_ERRDIR}/com_err.h ${COM_ERRDIR}/com_right.h
MAN3=	com_err.3
COM_ERRDIR=	${.CURDIR}/../../contrib/com_err
CFLAGS+=	-I${COM_ERRDIR}

SUBDIR=	doc

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${COM_ERRDIR}/com_err.h ${DESTDIR}/usr/include
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${COM_ERRDIR}/com_right.h ${DESTDIR}/usr/include

.include <bsd.lib.mk>

.PATH:	${COM_ERRDIR}
