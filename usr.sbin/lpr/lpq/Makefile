#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	lpq
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

BINGRP=	daemon
BINMODE=6555
BINDIR=	/usr/bin

.include <bsd.prog.mk>
