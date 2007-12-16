# $FreeBSD$

TESTS=	test-csqrt test-fenv test-ilogb test-lrint \
	test-lround test-nan test-next test-rem
CFLAGS+= -O0 -lm

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
