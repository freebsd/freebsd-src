# $FreeBSD$

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
