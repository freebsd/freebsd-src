# $FreeBSD: src/usr.bin/smbutil/Makefile,v 1.2.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	smbutil
SRCS=	smbutil.c dumptree.c login.c lookup.c view.c print.c

DPADD=	${LIBSMB} ${LIBKICONV}
LDADD=	-lsmb -lkiconv

CONTRIBDIR=	${.CURDIR}/../../contrib/smbfs
CFLAGS+=	-I${CONTRIBDIR}/include

.PATH:	${CONTRIBDIR}/smbutil

.include <bsd.prog.mk>
