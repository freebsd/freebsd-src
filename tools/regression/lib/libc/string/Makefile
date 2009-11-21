# $FreeBSD: src/tools/regression/lib/libc/string/Makefile,v 1.4.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

CFLAGS+=	-I/usr/local/include 
LDFLAGS+=	-L/usr/local/lib
LDLIBS=		-ltap

TESTS=	test-stpncpy test-strerror test-wcscasecmp test-wcsnlen

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
