# $FreeBSD$

.include "${.CURDIR}/../named/Makefile.inc"

SCRIPTS=	named.restart
MAN=		named.restart.8
CLEANFILES+=	${SCRIPTS}
SCRIPTSNAME=	${SCRIPTS}

named.restart:	named.restart.sh ${BIND_DIR}/Makefile
	sed -e "s|%INDOT%|${INDOT}|" \
	    -e "s|%DESTSBIN%|${DESTSBIN}|" \
	    < ${.CURDIR}/named.restart.sh > named.restart

.include <bsd.prog.mk>
