#	From: @(#)Makefile	1.8 (Berkeley) 6/8/93
#	$Id: Makefile,v 1.3 1995/01/22 16:49:22 bde Exp $

VOLUME=	papers
DOC=	memfs
SRCS=	tmac.srefs paper.t
MACROS=	-ms
REFER=	refer -n -e -l -s -p ${.CURDIR}/ref.bib
EXTRA=	ref.bib A.t tmac.srefs
CLEANFILES=ref.bib.i A.gt paper.t
SRCDIR=	${.OBJDIR}	# ack!

paper.t: 0.t 1.t ${.OBJDIR}/ref.bib.i ${.OBJDIR}/A.gt
	${REFER} ${.CURDIR}/0.t ${.CURDIR}/1.t A.gt > ${.TARGET}

${.OBJDIR}/ref.bib.i ref.bib.i: ${.CURDIR}/ref.bib
	${INDXBIB} -c ${.CURDIR}/../../../../contrib/groff/indxbib/eign -o ref.bib ${.CURDIR}/ref.bib

${.OBJDIR}/A.gt A.gt: ${.CURDIR}/A.t
	${GRIND} < ${.CURDIR}/A.t > A.gt

.include <bsd.doc.mk>
