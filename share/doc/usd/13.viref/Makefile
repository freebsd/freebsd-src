#	From: @(#)Makefile	8.16 (Berkeley) 8/15/94
# $FreeBSD$

VOLUME=	usd/13.viref
SRCS=	vi.ref
EXTRA=	ex.cmd.roff ref.so set.opt.roff vi.cmd.roff
OBJS=	index.so
MACROS=	-me -U
CLEANFILES+=index index.so
USE_SOELIM=	yes
USE_SOELIMPP=	yes
USE_TBL=	yes
SRCDIR=	${.CURDIR}/../../../../contrib/nvi/docs/USD.doc/vi.ref

# Build index.so as a side-effect of building the paper.
index.so: ${SRCS} ${EXTRA}
	(cd ${SRCDIR}; ${SOELIMPP} -e 's:^\.so index.so$$::' ${SRCS}) | \
	${ROFF} > /dev/null
	sed -e 's/MINUSSIGN/-/' \
	    -e 's/DOUBLEQUOTE/""/' \
	    -e "s/SQUOTE/'/" \
	    -e 's/ /__SPACE/g' < index | \
	sort -u '-t	' +0 -1 +1n | awk -f ${SRCDIR}/merge.awk | \
	sed -e 's/__SPACE/ /g' \
	    -e "s/^\\(['\\.]\\)/\\\\\&\\1/" > index.so 

.include <bsd.doc.mk>
