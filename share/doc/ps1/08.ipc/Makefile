#	from: @(#)Makefile	1.4 (Berkeley) 6/30/90
#	Makefile,v 1.5 1993/07/31 15:33:44 mycroft Exp

DIR=	ps1/08.ipc
DOC=	08.ipc

SRCS=	0.t 1.t 2.t 3.t 4.t 5.t
MACROS=	-ms

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS}
	( cd ${.CURDIR} ; ${TBL} ${SRCS} ) | ${ROFF} > ${.TARGET}
