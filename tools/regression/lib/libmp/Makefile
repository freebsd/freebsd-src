# $FreeBSD: src/tools/regression/lib/libmp/Makefile,v 1.1.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

TESTS=	test-libmp
CFLAGS+= -g -Wall -lcrypto -lmp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
