#	@(#)Makefile	5.6 (Berkeley) 3/5/91
#

PROG=	key
SRCS=	skey.c
MAN1=   key.1

CFLAGS+= -D_SKEY_INTERNAL

DPADD=	${LIBSKEY} ${LIBMD}
LDADD=	-lskey -lmd

.include <bsd.prog.mk>
