#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD$

DIR=	psd/06.Clang
SRCS=	Clang.ms
MACROS=	-ms
GROFF=	groff

all: paper.ps paper.ascii

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${GROFF} ${MACROS} - > ${.TARGET}

paper.ascii: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} ${MACROS} - > ${.TARGET}

paper.ascii.gz: paper.ascii
paper.ps.gz: paper.ps

paper.ascii.gz paper.ps.gz:
	gzip -f ${.TARGET:.gz=}

clean:
	rm -f paper.* *.spell errs Errs make.out Make.log *~

.include <bsd.doc.mk>
