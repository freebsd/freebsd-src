#	@(#)Makefile	5.5 (Berkeley) 7/1/90
# $FreeBSD$

PROG=	keyinfo
CFLAGS+=-Wall
DPADD=	${LIBSKEY} ${LIBMD}
LDADD=	-lskey -lmd
BINMODE=4555

.include <bsd.prog.mk>
