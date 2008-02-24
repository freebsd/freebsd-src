# $FreeBSD: src/tools/regression/lib/libmp/Makefile,v 1.1 2006/07/28 16:00:59 simon Exp $

TESTS=	test-libmp
CFLAGS+= -g -Wall -lcrypto -lmp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
