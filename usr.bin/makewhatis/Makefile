# $FreeBSD: src/usr.bin/makewhatis/Makefile,v 1.3.34.1 2008/11/25 02:59:29 kensmith Exp $

PROG=		makewhatis
DPADD=		${LIBZ}
LDADD=		-lz
SCRIPTS=	makewhatis.local.sh
MAN=		makewhatis.1 makewhatis.local.8
SCRIPTSDIR=	/usr/libexec
LINKS=		${SCRIPTSDIR}/makewhatis.local ${SCRIPTSDIR}/catman.local
MLINKS=		makewhatis.local.8 catman.local.8

.include <bsd.prog.mk>
