#
# $FreeBSD: src/gnu/usr.bin/perl/pod/pod2text/Makefile,v 1.2.2.1 2000/10/05 09:54:51 markm Exp $
#

PROG=	pod2text

NOMAN=	true

${PROG}: ${PROG}.PL
	${MINIPERL} ${MINIPERLOPT} ${PROG}.PL

.include <bsd.prog.mk>

${PROG}.PL: ${PERL5SRC}/pod/${PROG}.PL
	cp -p ${.OODATE} ${.TARGET}
