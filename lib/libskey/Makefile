#	@(#)Makefile	5.4 (Berkeley) 5/7/91
#	$FreeBSD: src/lib/libskey/Makefile,v 1.15 2000/01/14 07:57:40 rgrimes Exp $

LIB=	skey
SRCS=	skeyaccess.c put.c skey_crypt.c skey_getpass.c skeylogin.c skeysubr.c
INCS=	skey.h
MAN1=	skey.1
MAN3=	skey.3
MLINKS=	skey.3 skeylookup.3 skey.3 skeyverify.3 skey.3 skeychallenge.3 \
	skey.3 skeyinfo.3 skey.3 skeyaccess.3 skey.3 skey_getpass.3 \
	skey.3 skey_crypt.3
MAN5=	skey.access.5

CFLAGS+=-DPERMIT_CONSOLE -D_SKEY_INTERNAL -I${.CURDIR}
CFLAGS+=-W -Wall
.if ${MACHINE_ARCH} == "i386"
CFLAGS+=-Werror
.endif

DPADD+= ${LIBCRYPT} ${LIBMD}
LDADD+=	-lcrypt -lmd

.include <bsd.lib.mk>
