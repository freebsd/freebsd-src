# $FreeBSD: src/usr.bin/ipcrm/Makefile,v 1.7.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	ipcrm
SRCS=	ipcrm.c ipc.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
CFLAGS+=-I${.CURDIR}/../ipcs
.PATH:  ${.CURDIR}/../ipcs

.include <bsd.prog.mk>
