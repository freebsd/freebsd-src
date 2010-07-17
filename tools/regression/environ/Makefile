#
# $FreeBSD: src/tools/regression/environ/Makefile,v 1.1.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $
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
