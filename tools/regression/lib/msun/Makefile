# $FreeBSD: src/tools/regression/lib/msun/Makefile,v 1.6.10.1 2010/02/10 00:26:20 kensmith Exp $

TESTS=	test-fenv test-ilogb test-lrint test-lround test-next test-rem
CFLAGS+= -O0 -lm
ARCH!=	uname -m

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
