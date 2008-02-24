# $FreeBSD: src/tools/regression/lib/libc/net/Makefile,v 1.3 2007/05/13 14:03:21 rwatson Exp $

TESTS=	test-ether test-eui64_aton test-eui64_line test-eui64_ntoa
CFLAGS+= -g -Wall

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
