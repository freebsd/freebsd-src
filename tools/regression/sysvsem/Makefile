# $FreeBSD: src/tools/regression/sysvsem/Makefile,v 1.3.20.1 2009/04/15 03:14:26 kensmith Exp $
# SysV Semaphores Regression Utility 
# Obtained from: $NetBSD: Makefile,v 1.3 2002/03/05 21:57:00 thorpej Exp $

PROG=	semtest
NO_MAN=

regress:
	@if ./semtest ./semtest; then \
		echo "PASS"; \
	else \
		echo "FAIL"; \
	fi

.include <bsd.prog.mk>
