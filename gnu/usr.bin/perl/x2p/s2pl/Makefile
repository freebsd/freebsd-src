#
# $FreeBSD: src/gnu/usr.bin/perl/x2p/s2pl/Makefile,v 1.3 1999/08/27 23:36:35 peter Exp $
#

PROG=	s2p

${PROG}: ${PROG}.PL
	miniperl ${MINIPERLOPT} ${.OODATE}

${PROG}.1: ${PROG}.PL
	miniperl ${MINIPERLOPT} ${.OBJDIR}/../../pod/pod2man/pod2man ${.OODATE} > ${.TARGET}

CLEANFILES+=	${PROG}.1 ${PROG}.PL

.include <bsd.prog.mk>

${PROG}.PL: ${PERL5SRC}/x2p/${PROG}.PL
	@ln -sf ${.OODATE} ${.TARGET}
