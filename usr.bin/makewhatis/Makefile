# $FreeBSD$

PROG=		makewhatis
DPADD=		${LIBZ}
LDADD=		-lz
SCRIPTS=	makewhatis.local.sh
MAN=		makewhatis.1 makewhatis.local.8
SCRIPTSDIR=	/usr/libexec
LINKS=		${SCRIPTSDIR}/makewhatis.local ${SCRIPTSDIR}/catman.local
MLINKS=		makewhatis.local.8 catman.local.8

.include <bsd.prog.mk>
