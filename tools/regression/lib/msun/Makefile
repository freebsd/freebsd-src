# $FreeBSD$

TESTS=	test-csqrt test-fenv test-ilogb test-lrint \
	test-lround test-next test-rem
CFLAGS+= -O0 -lm
ARCH!=	uname -m

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
