#
# $FreeBSD: src/gnu/usr.bin/perl/pod/pod2html/Makefile,v 1.1.2.1 1999/08/29 14:34:19 peter Exp $
#

PROG=	pod2html

${PROG}: ${PROG}.PL
	miniperl ${MINIPERLOPT} ${PROG}.PL

${PROG}.1: ${PROG}.PL
	miniperl ${MINIPERLOPT} ${.OBJDIR}/../pod2man/pod2man ${PROG}.PL > ${PROG}.1

.include <bsd.prog.mk>

${PROG}.PL: ${PERL5SRC}/pod/${PROG}.PL
	cp -p ${.OODATE} ${.TARGET}
