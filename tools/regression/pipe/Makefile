#
# $FreeBSD: src/tools/regression/pipe/Makefile,v 1.1.4.1 2005/02/13 07:23:34 obrien Exp $
#
# "make" then "make regress".
#
PROG=	bigpipetest
NOMAN=

regress:
	@if ./bigpipetest; then \
		echo "PASS"; \
	else \
		echo "FAIL"; \
	fi

.include <bsd.prog.mk>
