#	@(#)Makefile	5.6 (Berkeley) 3/5/91
# $FreeBSD$

PROG=	keyinit
SRCS=	skeyinit.c

CFLAGS+= -D_SKEY_INTERNAL

BINMODE=4555

DPADD=	${LIBSKEY} ${LIBCRYPT} ${LIBMD}
LDADD=	-lskey -lcrypt -lmd

.include <bsd.prog.mk>
