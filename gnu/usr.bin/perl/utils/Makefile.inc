#
# $Id$
#

PERL5SRC=	${.CURDIR}/../../../../../contrib/perl5
MINIPERLOPT?=	-I${.OBJDIR}/../../miniperl/lib -I${.OBJDIR}/../../miniperl

${PROG}: ${PROG}.PL
	miniperl ${.OODATE}

${PROG}.1: ${PROG}.PL
	miniperl ${.OBJDIR}/../../pod/pod2man/pod2man ${.OODATE} > ${.TARGET}

${PROG}.PL: ${PERL5SRC}/utils/${PROG}.PL
	@ln -sf ${.OODATE} ${.TARGET}

CLEANFILES+=	${PROG}.1 ${PROG}.PL pstruct

STRIP=

.include "../Makefile.inc"
