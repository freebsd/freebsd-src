#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	pac
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
MAN=	pac.8
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
