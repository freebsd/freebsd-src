#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.PATH: ${.CURDIR}/../common_source

PROG=	pac
MAN=	pac.8

CFLAGS+= -I${.CURDIR}/../common_source ${CWARNFLAGS}

DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
