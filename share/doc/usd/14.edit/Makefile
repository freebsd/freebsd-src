#	from: @(#)Makefile	6.2 (Berkeley) 6/30/90
#	Makefile,v 1.5 1993/07/31 15:30:17 mycroft Exp

VOLUME=	usd
DOC=	14.edit

SRCS=	edittut.ms
MACROS=	-ms

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS}
	( cd ${.CURDIR} ; ${TBL} ${SRCS} ) | ${ROFF} > ${.TARGET}

# index for versatec is different from the one in edit.tut
# because the fonts are different and entries reference page
# rather than section numbers.  if you have a typesetter
# you should just use the index in edit.tut, and ignore editvindex.

editvindex:
	( cd ${.CURDIR} ; ${ROFF} -n22 edit.vindex ) > ${DOC}.${.TARGET}.${PRINTER}
