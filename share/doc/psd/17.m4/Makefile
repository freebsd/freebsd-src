#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD$

DIR=	psd/17.m4
SRCS=	m4.ms
MACROS=	-msU
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
