#	@(#)Makefile	8.2 (Berkeley) 2/28/1994

DIR=	smm/09.sendmail
SRCS=	intro.me
MACROS=	-me

all: intro.ps

intro.ps: ${SRCS}
	rm -f ${.TARGET}
	${PIC} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
