#	@(#)Makefile	8.1 (Berkeley) 8/14/93
# $FreeBSD$

VOLUME=	usd/21.troff
SRCS=	m0 m0a m1 m2 m3 m4 m5
APNDX=	table1 table2
MACROS= m.mac
GROFF=	groff

all: paper.ps apndx.ps paper.txt apndx.txt

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${GROFF} ${MACROS} - > ${.TARGET}

apndx.ps: ${APNDX}
	${TBL} ${APNDX} | ${GROFF} - > ${.TARGET}

paper.txt: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} ${MACROS} - > ${.TARGET}

apndx.txt: ${APNDX}
	${TBL} ${APNDX} | ${ROFF} - > ${.TARGET}

paper.ascii.gz: paper.txt apndx.txt
	cat paper.txt apndx.txt | gzip -c > ${.TARGET}

paper.ps.gz: paper.ps
	gzip -f paper.ps

clean:
	rm -f paper.* apndx.* *.spell errs Errs make.out

spell: ${SRCS} ${APNDX}
	@for i in ${PAPER} ${APNDX}; do \
		echo $$i; spell $$i | sort | comm -23 - spell.ok > $$i.spell; \
	done

.include <bsd.doc.mk>
