#	@(#)Makefile	8.2 (Berkeley) 2/28/94

SRCS=	usenix.me
MACROS=	-me

all: usenix.ps

usenix.ps: ${SRCS}
	rm -f ${.TARGET}
	${PIC} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
