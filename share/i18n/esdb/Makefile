# $FreeBSD$
# $NetBSD: Makefile,v 1.14 2007/04/01 18:52:30 tnozaki Exp $

FILESDIR= ${ESDBDIR}

SUBDIR=	APPLE AST BIG5 CP DEC EUC EBCDIC GB GEORGIAN ISO-2022 ISO-8859 \
	ISO646 KAZAKH KOI MISC TCVN UTF

FILES+=	esdb.dir esdb.dir.db esdb.alias esdb.alias.db
CLEANFILES= ${FILES}

esdb.dir: ${SUBDIR}
	> $@
.for i in ${SUBDIR}
	cat ${i}/esdb.dir.${i} >>${.TARGET}
.endfor
esdb.dir.db: esdb.dir
	${MKESDB} -m -o ${.TARGET} ${.ALLSRC}

esdb.alias: ${SUBDIR}
	> $@
.for i in ${SUBDIR}
	cat ${i}/esdb.alias.${i} >>${.TARGET}
.endfor
esdb.alias.db: esdb.alias
	${MKESDB} -m -o ${.TARGET} ${.ALLSRC}

all: ${FILES}
realall: ${FILES}

.include "./Makefile.inc"
.include <bsd.prog.mk>
