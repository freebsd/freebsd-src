#
# $FreeBSD: src/tools/regression/environ/Makefile,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $
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
