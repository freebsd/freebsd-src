# $FreeBSD: src/usr.bin/ipcrm/Makefile,v 1.7.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	ipcrm
SRCS=	ipcrm.c ipc.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
CFLAGS+=-I${.CURDIR}/../ipcs
.PATH:  ${.CURDIR}/../ipcs

.include <bsd.prog.mk>
