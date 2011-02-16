# $FreeBSD: src/tools/regression/lib/libc/nss/Makefile,v 1.1.14.1 2010/12/21 17:10:29 kensmith Exp $

TESTS=	test-getaddr test-getgr test-gethostby test-getpw test-getproto\
	test-getrpc test-getserv test-getusershell
CFLAGS+= -g -Wall

.PHONY: tests
tests: ${TESTS}

.PHONY: clean
clean:
	-rm -f ${TESTS}
