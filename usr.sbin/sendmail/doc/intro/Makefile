#	@(#)Makefile	8.2 (Berkeley) 2/28/94

DIR=	smm/09.sendmail
SRCS=	intro.me
MACROS=	-me
CLEANFILES= *.ps

all: intro.ps

intro.ps: ${SRCS}
	rm -f ${.TARGET}
	${PIC} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
