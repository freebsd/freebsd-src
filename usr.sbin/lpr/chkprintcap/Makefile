# $FreeBSD: src/usr.sbin/lpr/chkprintcap/Makefile,v 1.12 2002/06/23 20:57:16 gad Exp $

.PATH: ${.CURDIR}/../common_source

PROG=	chkprintcap
MAN=	chkprintcap.8
SRCS=	chkprintcap.c skimprintcap.c

CFLAGS+= -I${.CURDIR}/../common_source

DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
