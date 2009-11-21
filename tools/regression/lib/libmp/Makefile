# $FreeBSD: src/tools/regression/lib/libmp/Makefile,v 1.1.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

TESTS=	test-libmp
CFLAGS+= -g -Wall -lcrypto -lmp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
