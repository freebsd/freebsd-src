#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD$

VOLUME=	psd/02.implement
SRCS=	implement
MACROS=	-ms
# REFER=	refer -p /usr/old/dict/papers/Ind -e
GROFF=	groff

all: paper.ps paper.ascii

paper.ps: ${SRCS}
	${SOELIM} ${SRCS} | ${REFER} | ${PIC} | ${GROFF} ${MACROS} - > ${.TARGET}

paper.ascii: ${SRCS}
	${SOELIM} ${SRCS} | ${REFER} | ${PIC} | ${ROFF} ${MACROS} - > ${.TARGET}

paper.ascii.gz: paper.ascii
paper.ps.gz: paper.ps

paper.ascii.gz paper.ps.gz:
	gzip -f ${.TARGET:.gz=}

clean:
	rm -f paper.* *.spell errs Errs make.out

.include <bsd.doc.mk>
