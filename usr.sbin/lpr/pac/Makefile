#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/lpr/pac/Makefile,v 1.7 1999/08/28 01:16:56 peter Exp $

PROG=	pac
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
MAN8=	pac.8
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
