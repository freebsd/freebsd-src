#
# $FreeBSD: src/tools/regression/pipe/Makefile,v 1.3.24.1 2010/02/10 00:26:20 kensmith Exp $
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
