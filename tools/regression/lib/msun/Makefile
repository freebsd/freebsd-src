# $FreeBSD$

TESTS=	test-ctrig \
	test-exponential test-fma test-invtrig \
	test-lround
CFLAGS+= -O0 -lm -Wno-unknown-pragmas

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
