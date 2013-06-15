# $FreeBSD$
# $NetBSD: Makefile,v 1.13 2007/03/13 16:34:37 tnozaki Exp $

FILESDIR= ${CSMAPPERDIR}

SUBDIR=	APPLE AST BIG5 CNS CP EBCDIC GB GEORGIAN ISO646 ISO-8859 JIS \
	KAZAKH KOI KS MISC TCVN

mapper.dir: ${SUBDIR}
	> ${.TARGET}
.for i in ${SUBDIR}
	cat ${i}/mapper.dir.${i} >> ${.TARGET}
.endfor
mapper.dir.db: mapper.dir
	${MKCSMAPPER} -m -o ${.TARGET} ${.ALLSRC}

FILES+=	mapper.dir mapper.dir.db
CLEANFILES+= mapper.dir mapper.dir.db

charset.pivot: ${SUBDIR}
	> ${.TARGET}
.for i in ${SUBDIR}
	cat ${i}/charset.pivot.${i} >> ${.TARGET}
.endfor
charset.pivot.pvdb: charset.pivot
	${MKCSMAPPER} -p -o ${.TARGET} ${.ALLSRC}

FILES+=	charset.pivot charset.pivot.pvdb
CLEANFILES+= charset.pivot charset.pivot.pvdb

all: ${FILES}
realall: ${FILES}

.include "./Makefile.inc"
.include <bsd.prog.mk>
