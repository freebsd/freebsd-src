#	From: @(#)Makefile	8.16 (Berkeley) 8/15/94
#	$Id: Makefile,v 1.2 1995/05/30 06:58:01 rgrimes Exp $

VOLUME=	usd/13.viref
SRCS=	vi.ref
EXTRA=	ex.cmd.roff ../../install/recover.script ref.so set.opt.roff \
	vi.cmd.roff
OBJS=	index.so
MACROS=	-me
CLEANFILES+=index index.so
USE_SOELIM=	yes
USE_SOELIMPP=	yes
USE_TBL=	yes
SRCDIR=	${.CURDIR}/../../../../usr.bin/vi/USD.doc/vi.ref

index.so:: ${SRCS} ${EXTRA}
ALLSRCS=	${SRCS:S;^;${SRCDIR}/;}
index.so:: ${SRCS}
	# Build index.so, side-effect of building the paper.
	${SOELIMPP} -e 's:^\.so index.so$$::' ${ALLSRCS} | ${ROFF} > /dev/null
	sed -e 's/MINUSSIGN/\\-/' \
	    -e 's/DOUBLEQUOTE/""/' \
	    -e "s/SQUOTE/'/" \
	    -e 's/ /__SPACE/g' < index | \
	sort -u '-t	' +0 -1 +1n | awk -f ${SRCDIR}/merge.awk | \
	sed -e 's/__SPACE/ /g' > index.so

.include <bsd.doc.mk>
