#
# $FreeBSD$
#

PERL5SRC=	${.CURDIR}/../../../../../contrib/perl5
PERL5LIBSRC=	${.CURDIR}/../../libperl

.include "../Makefile.inc"

MINIPERLOPT?=	-I${.OBJDIR}/../../perl/lib -I${.OBJDIR}/../../perl

.if defined(SCRIPTS)
.if !defined(NOMAN)
MAN=	${SCRIPTS}.1
.endif

${SCRIPTS}: ${SCRIPTS}.PL
	${MINIPERL} ${MINIPERLOPT} ${.ALLSRC}

${SCRIPTS}.1: ${SCRIPTS}.PL
	${MINIPERL} ${MINIPERLOPT} \
		${.OBJDIR}/../pod2man/pod2man ${.ALLSRC} > ${.TARGET}

${SCRIPTS}.PL: ${PERL5SRC}/pod/${SCRIPTS}.PL
	ln -sf ${.ALLSRC} ${.TARGET}

CLEANFILES+=	${SCRIPTS} ${SCRIPTS}.1 ${SCRIPTS}.PL
.endif
