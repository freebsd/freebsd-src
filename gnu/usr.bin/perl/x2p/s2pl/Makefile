#
# $FreeBSD$
#

PROG=	s2p

${PROG}: ${PROG}.PL
	${MINIPERL} ${MINIPERLOPT} ${.OODATE}

${PROG}.1: ${PROG}.PL
	${MINIPERL} ${MINIPERLOPT} ${.OBJDIR}/../../pod/pod2man/pod2man ${.OODATE} > ${.TARGET}

CLEANFILES+=	${PROG}.1 ${PROG}.PL

.include <bsd.prog.mk>

${PROG}.PL: ${PERL5SRC}/x2p/${PROG}.PL
	@ln -sf ${.OODATE} ${.TARGET}
