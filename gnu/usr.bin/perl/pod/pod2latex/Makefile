#
# $FreeBSD: src/gnu/usr.bin/perl/pod/pod2latex/Makefile,v 1.2 1999/08/27 23:36:21 peter Exp $
#

PROG=	pod2latex

NOMAN=	true

${PROG}: ${PROG}.PL
	miniperl ${MINIPERLOPT} ${PROG}.PL

.include <bsd.prog.mk>

${PROG}.PL: ${PERL5SRC}/pod/${PROG}.PL
	cp -p ${.OODATE} ${.TARGET}
