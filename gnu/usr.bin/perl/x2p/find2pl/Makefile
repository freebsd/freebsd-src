#
# $FreeBSD: src/gnu/usr.bin/perl/x2p/find2pl/Makefile,v 1.3.2.1 2000/10/05 09:54:57 markm Exp $
#

PROG=	find2perl
NOMAN=	true

${PROG}: ${PROG}.PL
	${MINIPERL} ${MINIPERLOPT} ${.OODATE}

${PROG}.1: ${PROG}.PL
	${MINIPERL} ${MINIPERLOPT} ${.OBJDIR}/../../pod/pod2man/pod2man ${.OODATE} > ${.TARGET}

CLEANFILES+=	${PROG}.1 ${PROG}.PL

.include <bsd.prog.mk>

${PROG}.PL: ${PERL5SRC}/x2p/${PROG}.PL
	@ln -sf ${.OODATE} ${.TARGET}
