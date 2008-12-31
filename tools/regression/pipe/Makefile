#
# $FreeBSD: src/tools/regression/pipe/Makefile,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $
#
# "make" then "make regress".
#
PROG=	bigpipetest
NO_MAN=

regress:
	@if ./bigpipetest; then \
		echo "PASS"; \
	else \
		echo "FAIL"; \
	fi

.include <bsd.prog.mk>
