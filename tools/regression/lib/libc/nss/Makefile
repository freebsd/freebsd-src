# $FreeBSD: src/tools/regression/lib/libc/nss/Makefile,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $

TESTS=	test-getaddr test-getgr test-gethostby test-getpw test-getproto\
	test-getrpc test-getserv test-getusershell
CFLAGS+= -g -Wall

.PHONY: tests
tests: ${TESTS}

.PHONY: clean
clean:
	-rm -f ${TESTS}
