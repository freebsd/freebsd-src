#	@(#)Makefile	5.4 (Berkeley) 5/7/91

LIB=	skey
SRCS=	skeyaccess.c md4.c put.c skey_crypt.c skeylogin.c skeysubr.c
MAN5=	skey.access.5

CFLAGS+=-DMPU8086 -DPERMIT_CONSOLE -I${.CURDIR}

beforeinstall:
	-cd ${.CURDIR}; cmp -s skey.h ${DESTDIR}/usr/include/skey.h > \
	  /dev/null 2>&1 || \
	  install -c -o ${BINOWN} -g ${BINGRP} -m 444 skey.h \
	  ${DESTDIR}/usr/include

.include <bsd.lib.mk>
