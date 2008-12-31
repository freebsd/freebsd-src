# $FreeBSD: src/tools/regression/lib/libmp/Makefile,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $

TESTS=	test-libmp
CFLAGS+= -g -Wall -lcrypto -lmp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
