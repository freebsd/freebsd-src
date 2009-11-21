# $FreeBSD: src/tools/regression/lib/libc/nss/Makefile,v 1.1.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

TESTS=	test-getaddr test-getgr test-gethostby test-getpw test-getproto\
	test-getrpc test-getserv test-getusershell
CFLAGS+= -g -Wall

.PHONY: tests
tests: ${TESTS}

.PHONY: clean
clean:
	-rm -f ${TESTS}
