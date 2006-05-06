#	$NetBSD: Makefile,v 1.1 2002/03/01 11:21:58 ad Exp $
# $FreeBSD: src/usr.bin/pkill/Makefile,v 1.5 2004/11/13 17:12:22 ru Exp $

PROG=	pkill
WARNS?=	5

DPADD=	${LIBKVM}
LDADD=	-lkvm

LINKS=	${BINDIR}/pkill ${BINDIR}/pgrep
MLINKS=	pkill.1 pgrep.1

.include <bsd.prog.mk>
