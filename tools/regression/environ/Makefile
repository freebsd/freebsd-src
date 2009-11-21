#
# $FreeBSD: src/tools/regression/environ/Makefile,v 1.1.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $
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
