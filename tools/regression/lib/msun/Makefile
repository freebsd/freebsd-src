# $FreeBSD: src/tools/regression/lib/msun/Makefile,v 1.6.8.1 2009/04/15 03:14:26 kensmith Exp $

TESTS=	test-fenv test-ilogb test-lrint test-lround test-next test-rem
CFLAGS+= -O0 -lm
ARCH!=	uname -m

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
