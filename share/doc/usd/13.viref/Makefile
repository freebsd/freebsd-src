#	From: @(#)Makefile	8.16 (Berkeley) 8/15/94
#	$Id$

VOLUME=	usd/13.viref
SRCS=	vi.ref ${.OBJDIR}/index.so
FILES=	${SRCS} ex.cmd.roff set.opt.roff vi.cmd.roff 
MACROS=	-me
CLEANFILES+=vi.ref.txt index index.so
USE_SOELIM=	yes
USE_TBL=	yes
SRCDIR=	${.CURDIR}/../../../../usr.bin/vi/USD.doc/vi.ref

# some day, this will actually work...
${.OBJDIR}/index.so: vi.ref
	# Build index.so, side-effect of building the paper.
	(cd ${SRCDIR}; soelim vi.ref) | ${TBL} | ${ROFF} > /dev/null
	sed -e 's/MINUSSIGN/\\-/' \
	    -e 's/DOUBLEQUOTE/""/' \
	    -e "s/SQUOTE/'/" \
	    -e 's/ /__SPACE/g' < index | \
	sort -u '-t	' +0 -1 +1n | awk -f ${SRCDIR}/merge.awk | \
	sed -e 's/__SPACE/ /g' > index.so
	rm -f index

.include <bsd.doc.mk>
