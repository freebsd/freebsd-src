#	from: @(#)Makefile	6.3 (Berkeley) 5/7/91
#	Makefile,v 1.2 1993/07/31 15:33:08 mycroft Exp

VOLUME=	smm
DOC=	01.setup

RSRC=	../common/renohints.t
CSRC=	../common/3.t ../common/4.t ../common/5.t ../common/6.t 
SRC=	0.t 1.t 2.t ${CSRC} a.t b.t c.t
MACROS=	-ms

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRC} ${DOC}.vaxhints.${PRINTER} \
			${DOC}.renohints.${PRINTER}
	( cd ${.CURDIR} ; ${EQN} ${SRC} ) | ${TBL} | ${ROFF} > ${.TARGET}

${DOC}.vaxhints.${PRINTER}: vaxhints.t
	( cd ${.CURDIR} ; ${ROFF} vaxhints.t ) > ${.TARGET}

${DOC}.renohints.${PRINTER}: ${RSRC}
	( cd ${.CURDIR} ; ${TBL} ${RSRC} ) | ${ROFF} > ${.TARGET}
