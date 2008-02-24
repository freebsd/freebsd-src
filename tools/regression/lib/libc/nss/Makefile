# $FreeBSD: src/tools/regression/lib/libc/nss/Makefile,v 1.1 2007/04/15 11:02:31 bushman Exp $

TESTS=	test-getaddr test-getgr test-gethostby test-getpw test-getproto\
	test-getrpc test-getserv test-getusershell
CFLAGS+= -g -Wall

.PHONY: tests
tests: ${TESTS}

.PHONY: clean
clean:
	-rm -f ${TESTS}
