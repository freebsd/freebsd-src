#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/lpr/pac/Makefile,v 1.7.2.1 2001/04/25 12:09:57 ru Exp $

PROG=	pac
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
MAN=	pac.8
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
