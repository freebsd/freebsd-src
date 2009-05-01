#
# $FreeBSD: src/tools/regression/environ/Makefile,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $
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
