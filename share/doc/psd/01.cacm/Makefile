#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD$

VOLUME=	psd/01.cacm
SRCS=	p.mac p1 p2 p3 p4 p5 p6
MACROS=	-ms
# REFER=	refer -p /usr/old/dict/papers/Ind -e
GROFF=	groff

all: paper.ps paper.ascii

paper.ps: ${SRCS}
	${REFER} ${SRCS} | ${TBL} | ${GROFF} ${MACROS} - > ${.TARGET}

paper.ascii: ${SRCS}
	${REFER} ${SRCS} | ${TBL} | ${ROFF} ${MACROS} - > ${.TARGET}

paper.ascii.gz: paper.ascii
paper.ps.gz: paper.ps

paper.ascii.gz paper.ps.gz:
	gzip -f ${.TARGET:.gz=}

clean:
	rm -f paper.* *.spell errs Errs make.out

.include <bsd.doc.mk>
