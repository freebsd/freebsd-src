# $FreeBSD: src/tools/regression/lib/libmp/Makefile,v 1.1.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

TESTS=	test-libmp
CFLAGS+= -g -Wall -lcrypto -lmp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
