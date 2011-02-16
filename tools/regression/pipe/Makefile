#
# $FreeBSD: src/tools/regression/pipe/Makefile,v 1.3.26.1 2010/12/21 17:10:29 kensmith Exp $
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
