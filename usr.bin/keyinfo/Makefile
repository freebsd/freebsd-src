#	@(#)Makefile	5.5 (Berkeley) 7/1/90
# $FreeBSD$

PROG=	keyinfo
DPADD=	${LIBSKEY} ${LIBCRYPT} ${LIBMD}
LDADD=	-lskey -lcrypt -lmd
BINMODE=4555
WARNS?=1

.include <bsd.prog.mk>
