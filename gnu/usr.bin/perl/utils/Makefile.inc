#
# $FreeBSD: src/gnu/usr.bin/perl/utils/Makefile.inc,v 1.3 1999/08/27 23:36:25 peter Exp $
#

PERL5SRC=	${.CURDIR}/../../../../../contrib/perl5
MINIPERLOPT?=	-I${.OBJDIR}/../../perl/lib -I${.OBJDIR}/../../perl

${PROG}: ${PROG}.PL
	miniperl ${MINIPERLOPT} ${.OODATE}

${PROG}.1: ${PROG}.PL
	miniperl ${MINIPERLOPT} ${.OBJDIR}/../../pod/pod2man/pod2man ${.OODATE} > ${.TARGET}

${PROG}.PL: ${PERL5SRC}/utils/${PROG}.PL
	@ln -sf ${.OODATE} ${.TARGET}

CLEANFILES+=	${PROG}.1 ${PROG}.PL pstruct

STRIP=

.include "../Makefile.inc"
