#	@(#)Makefile	5.5 (Berkeley) 7/1/90
# $FreeBSD$

PROG=	keyinfo
CFLAGS+=-Wall
DPADD=	${LIBSKEY} ${LIBCRYPT} ${LIBMD}
LDADD=	-lskey -lcrypt -lmd
BINMODE=4555

.include <bsd.prog.mk>
