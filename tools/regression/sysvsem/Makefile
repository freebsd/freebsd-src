# $FreeBSD: src/tools/regression/sysvsem/Makefile,v 1.3.16.1 2008/10/02 02:57:24 kensmith Exp $
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
