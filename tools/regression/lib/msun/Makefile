# $FreeBSD: src/tools/regression/lib/msun/Makefile,v 1.1 2004/06/11 03:22:34 das Exp $

TESTS=	test-fenv
CFLAGS+= -O0 -lm
ARCH!=	uname -m
.if ${ARCH} == "alpha"
CFLAGS+= -mieee-with-inexact -mfp-rounding-mode=d
.endif

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
