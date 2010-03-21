# $FreeBSD: src/tools/regression/lib/libc/string/Makefile,v 1.3.22.1 2010/02/10 00:26:20 kensmith Exp $

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
