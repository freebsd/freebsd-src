#
# $FreeBSD: src/gnu/usr.bin/perl/pod/pod2text/Makefile,v 1.2 1999/08/27 23:36:22 peter Exp $
#

PROG=	pod2text

NOMAN=	true

${PROG}: ${PROG}.PL
	miniperl ${MINIPERLOPT} ${PROG}.PL

.include <bsd.prog.mk>

${PROG}.PL: ${PERL5SRC}/pod/${PROG}.PL
	cp -p ${.OODATE} ${.TARGET}
