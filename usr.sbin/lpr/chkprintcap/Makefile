# $FreeBSD: src/usr.sbin/lpr/chkprintcap/Makefile,v 1.12.38.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../common_source

PROG=	chkprintcap
MAN=	chkprintcap.8
SRCS=	chkprintcap.c skimprintcap.c

CFLAGS+= -I${.CURDIR}/../common_source

DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
