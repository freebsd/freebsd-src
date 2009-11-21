#
# $FreeBSD: src/tools/regression/pipe/Makefile,v 1.3.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $
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
