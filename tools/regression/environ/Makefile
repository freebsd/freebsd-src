#
# $FreeBSD: src/tools/regression/environ/Makefile,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
#
PROGS=	envctl retention timings

all clean test:
.for target in ${.TARGET}
.for prog in ${PROGS}
	@${MAKE} -f Makefile.${prog} ${target}
.endfor
.if make(clean)
	rm -f *~
.endif
.endfor
