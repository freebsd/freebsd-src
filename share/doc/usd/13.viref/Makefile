#	From: @(#)Makefile	8.16 (Berkeley) 8/15/94
# $FreeBSD$

VOLUME=		usd/13.viref
SRCS=		vi.ref
EXTRA=		ex.cmd.roff ref.so set.opt.roff vi.cmd.roff index.so
MACROS=		-me
CLEANFILES=	index index.so
TRFLAGS=	-U		# this is to hide warnings only
USE_SOELIM=	YES
USE_TBL=	YES
SRCDIR=		${.CURDIR}/../../../../contrib/nvi/docs/USD.doc/vi.ref

# Build index.so as a side-effect of building the paper.
index.so: ${SRCS} ${EXTRA:Nindex.so}
	(cd ${SRCDIR}; sed -e 's:^\.so index.so$$::' ${SRCS}) | ${ROFF} -U -z
	sed -e 's/MINUSSIGN/-/' \
	    -e 's/DOUBLEQUOTE/""/' \
	    -e "s/SQUOTE/'/" \
	    -e 's/ /__SPACE/g' < index | \
	sort -u '-t	' -k 1,1 -k 2n | awk -f ${SRCDIR}/merge.awk | \
	sed -e 's/__SPACE/ /g' \
	    -e "s/^\\(['\\.]\\)/\\\\\&\\1/" > index.so

.include <bsd.doc.mk>
