#	@(#)Makefile	8.1 (Berkeley) 8/14/93
# $FreeBSD$

VOLUME=	psd/15.yacc
SRCS=	ss.. ss0 ss1 ss2 ss3 ss4 ss5 ss6 ss7 ss8 ss9 ssA ssB ssa ssb ssc ssd
MACROS=	-ms
GROFF=	groff

all: paper.ps paper.ascii

paper.ps: ${SRCS}
	${REFER} ${SRCS} | ${GROFF} ${MACROS} - > ${.TARGET}

paper.ascii: ${SRCS}
	${REFER} ${SRCS} | ${ROFF} ${MACROS} - > ${.TARGET}

paper.ascii.gz: paper.ascii
paper.ps.gz: paper.ps

paper.ascii.gz paper.ps.gz:
	gzip -f ${.TARGET:.gz=}

clean:
	rm -f paper.* *.spell errs Errs make.out

.include <bsd.doc.mk>
