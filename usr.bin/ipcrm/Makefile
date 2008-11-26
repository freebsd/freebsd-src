# $FreeBSD: src/usr.bin/ipcrm/Makefile,v 1.6.14.1.2.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	ipcrm
SRCS=	ipcrm.c ipc.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
CFLAGS+=-I${.CURDIR}/../ipcs
.PATH:  ${.CURDIR}/../ipcs

.include <bsd.prog.mk>
