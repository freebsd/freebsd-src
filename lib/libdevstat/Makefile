
LIB=	devstat
SRCS=	devstat.c

MAN3=	devstat.3

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/devstat.h \
	    ${DESTDIR}/usr/include

CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../sys

.include <bsd.lib.mk>
