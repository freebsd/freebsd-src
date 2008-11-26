# $FreeBSD: src/usr.sbin/lpr/chkprintcap/Makefile,v 1.12.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../common_source

PROG=	chkprintcap
MAN=	chkprintcap.8
SRCS=	chkprintcap.c skimprintcap.c

CFLAGS+= -I${.CURDIR}/../common_source

DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
