#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD$

VOLUME=	psd/04.uprog
SRCS=	p.mac p0 p1 p2 p3 p4 p5 p6 p8 p9
MACROS=	-ms
GROFF=	groff

all: paper.ps paper.ascii

paper.ps: ${SRCS}
	${GROFF} ${MACROS} ${SRCS} > ${.TARGET}

paper.ascii: ${SRCS}
	${ROFF} ${MACROS} ${SRCS} > ${.TARGET}

paper.ascii.gz: paper.ascii
paper.ps.gz: paper.ps

paper.ascii.gz paper.ps.gz:
	gzip -f ${.TARGET:.gz=}

clean:
	rm -f paper.* *.spell errs Errs make.out Make.log *~

.include <bsd.doc.mk>
