#
# $FreeBSD: src/gnu/usr.bin/perl/pod/pod2man/Makefile,v 1.1.2.1 1999/08/29 14:34:21 peter Exp $
#

PROG=	pod2man

${PROG}: ${PROG}.PL
	miniperl ${MINIPERLOPT} ${PROG}.PL

${PROG}.1: ${PROG}.PL
	miniperl ${MINIPERLOPT} ${.OBJDIR}/../pod2man/pod2man ${PROG}.PL \
		> ${PROG}.1

.include <bsd.prog.mk>

${PROG}.PL: ${PERL5SRC}/pod/${PROG}.PL
	cp -p ${.OODATE} ${.TARGET}
