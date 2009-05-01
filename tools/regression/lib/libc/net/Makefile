# $FreeBSD: src/tools/regression/lib/libc/net/Makefile,v 1.3.8.1 2009/04/15 03:14:26 kensmith Exp $

TESTS=	test-ether test-eui64_aton test-eui64_line test-eui64_ntoa
CFLAGS+= -g -Wall

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
