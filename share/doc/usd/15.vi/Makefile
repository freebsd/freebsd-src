#	from: @(#)Makefile	6.3 (Berkeley) 6/30/90
#	Makefile,v 1.4 1993/07/31 15:30:11 mycroft Exp

DIR=	usd/15.vi
DOC=	15.vi

SRCS=	vi.in vi.chars
MACROS=	-ms

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS} ${DOC}.summary.${PRINTER} ${DOC}.apwh.${PRINTER}
	( cd ${.CURDIR} ; ${TBL} ${SRCS} ) | ${ROFF} > ${.TARGET}

${DOC}.summary.${PRINTER}: vi.summary
	( cd ${.CURDIR} ; ${TBL} vi.summary ) | ${ROFF}  > ${.TARGET}

${DOC}.apwh.${PRINTER}: vi.apwh.ms
	( cd ${.CURDIR} ; ${ROFF} vi.apwh.ms ) > ${.TARGET}
