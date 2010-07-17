#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/lpr/lpd/Makefile,v 1.14.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	lpd
MAN=	lpd.8
SRCS=	lpd.c printjob.c recvjob.c lpdchar.c modes.c

CFLAGS+= -I${.CURDIR}/../common_source

DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
