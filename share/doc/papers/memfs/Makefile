#	From: @(#)Makefile	1.8 (Berkeley) 6/8/93
# $FreeBSD: src/share/doc/papers/memfs/Makefile,v 1.9 1999/08/28 00:18:12 peter Exp $

VOLUME=	papers
DOC=	memfs
SRCS=	tmac.srefs paper.t
MACROS=	-ms
CLEANFILES=A.gt paper.t ref.bib.i
SRCDIR=	${.OBJDIR}	# ack!

A.gt: A.t
	${GRIND} < ${.ALLSRC} > ${.TARGET}

paper.t: 0.t 1.t ref.bib.i A.gt
	${REFER} -n -e -l -s -p ${.CURDIR}/ref.bib ${.ALLSRC:Nref.bib.i} \
	    > ${.TARGET}

ref.bib.i: ref.bib
	${INDXBIB} -c ${.CURDIR}/../../../../contrib/groff/indxbib/eign \
	    -o ref.bib ${.ALLSRC}

.include <bsd.doc.mk>
