#
# $FreeBSD$
#

PROG=	pod2text

NOMAN=	true

${PROG}: ${PROG}.PL
	${MINIPERL} ${MINIPERLOPT} ${PROG}.PL

.include <bsd.prog.mk>

${PROG}.PL: ${PERL5SRC}/pod/${PROG}.PL
	cp -p ${.OODATE} ${.TARGET}
