LIB=		ftpio
CFLAGS+=	-I${.CURDIR}
SRCS=		ftpio.c
MAN3=		ftpio.3

beforeinstall:
	cd ${.CURDIR}; cmp -s ftpio.h ${DESTDIR}/usr/include/ftpio.h || \
	  ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ftpio.h ${DESTDIR}/usr/include

.include <bsd.lib.mk>
