#
# $FreeBSD: src/tools/regression/pipe/Makefile,v 1.3.20.1 2009/04/15 03:14:26 kensmith Exp $
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
