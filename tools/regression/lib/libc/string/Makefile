# $FreeBSD$

CFLAGS=		-I/usr/local/include -L/usr/local/lib
LDFLAGS=	-ltap

TESTS=	test-strerror

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
