#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD$

DIR=	psd/03.iosys
SRCS=	iosys
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
	rm -f paper.* *.spell errs Errs make.out

.include <bsd.doc.mk>
