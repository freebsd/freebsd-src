# $FreeBSD: src/usr.bin/smbutil/Makefile,v 1.2.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	smbutil
SRCS=	smbutil.c dumptree.c login.c lookup.c view.c print.c

DPADD=	${LIBSMB} ${LIBKICONV}
LDADD=	-lsmb -lkiconv

CONTRIBDIR=	${.CURDIR}/../../contrib/smbfs
CFLAGS+=	-I${CONTRIBDIR}/include

.PATH:	${CONTRIBDIR}/smbutil

.include <bsd.prog.mk>
