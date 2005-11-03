# $FreeBSD: src/tools/regression/lib/libc/string/Makefile,v 1.3 2005/02/01 09:26:34 nik Exp $

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
