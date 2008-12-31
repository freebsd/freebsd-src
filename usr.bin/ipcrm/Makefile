# $FreeBSD: src/usr.bin/ipcrm/Makefile,v 1.6.24.1.2.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	ipcrm
SRCS=	ipcrm.c ipc.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
CFLAGS+=-I${.CURDIR}/../ipcs
.PATH:  ${.CURDIR}/../ipcs

.include <bsd.prog.mk>
