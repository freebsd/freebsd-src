#
# $FreeBSD: src/tools/regression/environ/Makefile,v 1.1.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $
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
