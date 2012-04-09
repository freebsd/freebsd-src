# $FreeBSD: src/tools/regression/lib/libc/net/Makefile,v 1.3.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

TESTS=	test-ether test-eui64_aton test-eui64_line test-eui64_ntoa
CFLAGS+= -g -Wall

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
