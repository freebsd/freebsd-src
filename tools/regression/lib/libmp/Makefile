# $FreeBSD: src/tools/regression/lib/libmp/Makefile,v 1.1.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

TESTS=	test-libmp
CFLAGS+= -g -Wall -lcrypto -lmp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
