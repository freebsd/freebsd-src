# $FreeBSD: src/usr.bin/nfsstat/Makefile,v 1.1.1.1.14.1 2001/05/25 23:42:08 tmm Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	nfsstat
CFLAGS+=-DNFS
DPADD=  ${LIBKVM}
LDADD= -lkvm

.include <bsd.prog.mk>
