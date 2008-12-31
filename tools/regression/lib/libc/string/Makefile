# $FreeBSD: src/tools/regression/lib/libc/string/Makefile,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $

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
