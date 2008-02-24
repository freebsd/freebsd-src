#
# $FreeBSD: src/tools/regression/environ/Makefile,v 1.1 2007/07/04 00:00:39 scf Exp $
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
