#	@(#)Makefile	5.5 (Berkeley) 7/1/90
# $FreeBSD$

BINMODE=4555
PROG=	keyinfo
LDFLAGS=-lskey -lcrypt -lmd
DPADD=	${LIBSKEY} ${LIBCRYPT} ${LIBMD}
CFLAGS+=-Wall

.include <bsd.prog.mk>
