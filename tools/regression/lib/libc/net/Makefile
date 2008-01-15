# $FreeBSD: src/tools/regression/lib/libc/net/Makefile,v 1.2 2005/02/12 18:09:20 ru Exp $

TESTS=	test-eui64_aton test-eui64_line test-eui64_ntoa
CFLAGS+= -g -Wall

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
