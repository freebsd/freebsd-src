#	@(#)Makefile	5.4 (Berkeley) 5/7/91

LIB=	skey
SRCS=	skeyaccess.c put.c skey_crypt.c skey_getpass.c skeylogin.c skeysubr.c
MAN1=	skey.1
MAN3=	skey.3
MLINKS=	skey.3 skeylookup.3 skey.3 skeyverify.3 skey.3 skeychallenge.3 \
	skey.3 skeyinfo.3 skey.3 skeyaccess.3 skey.3 skey_getpass.3 \
	skey.3 skey_crypt.3
MAN5=	skey.access.5

CFLAGS+=-DPERMIT_CONSOLE -D_SKEY_INTERNAL -I${.CURDIR}

.if ${BINFORMAT} == elf
DPADD+= ${LIBCRYPT} ${LIBMD}
LDADD+=	-lcrypt -lmd
.endif

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/skey.h \
		${DESTDIR}/usr/include

.include <bsd.lib.mk>
