# $FreeBSD$

CFLAGS+=	-I/usr/local/include 
LDFLAGS+=	-L/usr/local/lib
LDLIBS=		-ltap

TESTS=	test-strerror

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
