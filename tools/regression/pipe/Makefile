#
# $FreeBSD: src/tools/regression/pipe/Makefile,v 1.1 2003/07/31 12:40:56 pb Exp $
#
# "make" then "make regress".
#
PROG=	bigpipetest
NOMAN=	yes

regress:
	@if ./bigpipetest; then \
		echo "PASS"; \
	else \
		echo "FAIL"; \
	fi

.include <bsd.prog.mk>
