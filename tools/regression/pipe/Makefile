#
# $FreeBSD: src/tools/regression/pipe/Makefile,v 1.3 2004/12/21 08:47:23 ru Exp $
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
