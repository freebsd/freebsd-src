#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/lpr/lpd/Makefile,v 1.10.2.1 2001/04/25 12:09:54 ru Exp $

PROG=	lpd
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
MAN=	lpd.8
SRCS=	lpd.c printjob.c recvjob.c lpdchar.c modes.c
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
