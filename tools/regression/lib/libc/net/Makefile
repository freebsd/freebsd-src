# $FreeBSD: src/tools/regression/lib/libc/net/Makefile,v 1.1 2004/05/26 22:58:06 brooks Exp $

TESTS=	test-eui64_aton test-eui64_line test-eui64_ntoa
CFLAGS=	-g -Wall

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
