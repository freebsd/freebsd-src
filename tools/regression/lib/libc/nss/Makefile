# $FreeBSD: src/tools/regression/lib/libc/nss/Makefile,v 1.1.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

TESTS=	test-getaddr test-getgr test-gethostby test-getpw test-getproto\
	test-getrpc test-getserv test-getusershell
CFLAGS+= -g -Wall

.PHONY: tests
tests: ${TESTS}

.PHONY: clean
clean:
	-rm -f ${TESTS}
