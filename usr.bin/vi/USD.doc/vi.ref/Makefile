#	@(#)Makefile	8.16 (Berkeley) 8/15/94

DIR=	usd/13.viref
SRCS=	vi.ref ex.cmd.roff set.opt.roff vi.cmd.roff ref.so
MACROS=	-me
CLEANFILES+=vi.ref.txt index index.so

paper.ps: vi.ref index.so
	soelim vi.ref | ${TBL} | ${ROFF} > ${.TARGET}

vi.ref.txt: vi.ref index.so
	soelim vi.ref | ${TBL} | groff ${MACROS} -Tascii > $@

index.so: vi.ref
	# Build index.so, side-effect of building the paper.
	soelim vi.ref | ${TBL} | ${ROFF} > /dev/null
	sed -e 's/MINUSSIGN/\\-/' \
	    -e 's/DOUBLEQUOTE/""/' \
	    -e "s/SQUOTE/'/" \
	    -e 's/ /__SPACE/g' < index | \
	sort -u '-t	' +0 -1 +1n | awk -f merge.awk | \
	sed -e 's/__SPACE/ /g' > index.so
	rm -f index

.include <bsd.doc.mk>
