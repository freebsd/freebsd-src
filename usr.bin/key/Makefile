#	@(#)Makefile	5.6 (Berkeley) 3/5/91
# $FreeBSD$

PROG=	key
SRCS=	skey.c

CFLAGS+= -D_SKEY_INTERNAL

DPADD=	${LIBSKEY} ${LIBCRYPT} ${LIBMD}
LDADD=	-lskey -lcrypt -lmd

.include <bsd.prog.mk>
