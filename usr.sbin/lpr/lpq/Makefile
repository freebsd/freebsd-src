#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

BINDIR=	/usr/bin

PROG=	lpq
BINGRP=	daemon
BINMODE= 6555

CFLAGS+= -I${.CURDIR}/../common_source ${CWARNFLAGS}

DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
