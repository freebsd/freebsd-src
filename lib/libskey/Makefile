#	@(#)Makefile	5.4 (Berkeley) 5/7/91

LIB=	skey
SRCS=	skeyaccess.c put.c skey_crypt.c skey_getpass.c skeylogin.c skeysubr.c
MAN1=	skey.1
MAN5=	skey.access.5

CFLAGS+=-DPERMIT_CONSOLE -I${.CURDIR}

beforeinstall:
	-cd ${.CURDIR}; cmp -s skey.h ${DESTDIR}/usr/include/skey.h || \
	  ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 skey.h \
	  ${DESTDIR}/usr/include

.include <bsd.lib.mk>
