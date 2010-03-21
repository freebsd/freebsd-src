# $FreeBSD: src/tools/regression/lib/libmp/Makefile,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $

TESTS=	test-libmp
CFLAGS+= -g -Wall -lcrypto -lmp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
