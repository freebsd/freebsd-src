LIB=	vgl
SHLIB_MAJOR= 1
SHLIB_MINOR= 0
CFLAGS+=-Wall -I${.CURDIR}
SRCS=	main.c simple.c bitmap.c text.c mouse.c
MAN3=	vgl.3

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/vgl.h \
		${DESTDIR}/usr/include

.include <bsd.lib.mk>
