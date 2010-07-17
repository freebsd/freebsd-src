#
# $FreeBSD: src/tools/regression/pipe/Makefile,v 1.3.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $
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
