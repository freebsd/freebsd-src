# $FreeBSD: src/tools/regression/lib/libmp/Makefile,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $

TESTS=	test-libmp
CFLAGS+= -g -Wall -lcrypto -lmp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
