#
# $Id: Makefile,v 1.1 1998/09/09 07:20:27 markm Exp $
#

PROG=	pod2latex

NOMAN=	true

${PROG}: ${PROG}.PL
	miniperl ${MINIPERLOPT} ${PROG}.PL

.include <bsd.prog.mk>

${PROG}.PL: ${PERL5SRC}/pod/${PROG}.PL
	cp -p ${.OODATE} ${.TARGET}
