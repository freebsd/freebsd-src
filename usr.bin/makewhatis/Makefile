# $FreeBSD: src/usr.bin/makewhatis/Makefile,v 1.3.36.1 2009/04/15 03:14:26 kensmith Exp $

PROG=		makewhatis
DPADD=		${LIBZ}
LDADD=		-lz
SCRIPTS=	makewhatis.local.sh
MAN=		makewhatis.1 makewhatis.local.8
SCRIPTSDIR=	/usr/libexec
LINKS=		${SCRIPTSDIR}/makewhatis.local ${SCRIPTSDIR}/catman.local
MLINKS=		makewhatis.local.8 catman.local.8

.include <bsd.prog.mk>
