#	@(#)Makefile	5.6 (Berkeley) 3/5/91
#

PROG=	key
SRCS=	skey.c
MAN1=   key.1

CFLAGS+= -D_SKEY_INTERNAL

DPADD=	${LIBSKEY} ${LIBCRYPT} ${LIBMD}
LDADD=	-lskey -lcrypt -lmd

.include <bsd.prog.mk>
