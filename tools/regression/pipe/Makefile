#
# $FreeBSD$
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
