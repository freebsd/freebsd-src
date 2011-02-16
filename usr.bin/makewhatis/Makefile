# $FreeBSD: src/usr.bin/makewhatis/Makefile,v 1.3.42.1 2010/12/21 17:10:29 kensmith Exp $

PROG=		makewhatis
DPADD=		${LIBZ}
LDADD=		-lz
SCRIPTS=	makewhatis.local.sh
MAN=		makewhatis.1 makewhatis.local.8
SCRIPTSDIR=	/usr/libexec
LINKS=		${SCRIPTSDIR}/makewhatis.local ${SCRIPTSDIR}/catman.local
MLINKS=		makewhatis.local.8 catman.local.8

.include <bsd.prog.mk>
