#
# $Id: Makefile,v 1.2 1998/09/10 10:51:11 markm Exp $
#

PROG=	find2perl
NOMAN=	true

${PROG}: ${PROG}.PL
	miniperl ${MINIPERLOPT} ${.OODATE}

${PROG}.1: ${PROG}.PL
	miniperl ${MINIPERLOPT} ${.OBJDIR}/../../pod/pod2man/pod2man ${.OODATE} > ${.TARGET}

CLEANFILES+=	${PROG}.1 ${PROG}.PL

.include <bsd.prog.mk>

${PROG}.PL: ${PERL5SRC}/x2p/${PROG}.PL
	@ln -sf ${.OODATE} ${.TARGET}
