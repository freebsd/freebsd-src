# $FreeBSD: src/tools/regression/lib/libc/nss/Makefile,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $

TESTS=	test-getaddr test-getgr test-gethostby test-getpw test-getproto\
	test-getrpc test-getserv test-getusershell
CFLAGS+= -g -Wall

.PHONY: tests
tests: ${TESTS}

.PHONY: clean
clean:
	-rm -f ${TESTS}
