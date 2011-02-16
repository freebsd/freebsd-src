# $FreeBSD: src/tools/regression/lib/libc/string/Makefile,v 1.3.24.1 2010/12/21 17:10:29 kensmith Exp $

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
