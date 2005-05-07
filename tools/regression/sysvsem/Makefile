# $FreeBSD: src/tools/regression/sysvsem/Makefile,v 1.1.8.1 2005/02/13 07:23:35 obrien Exp $
# SysV Semaphores Regression Utility 
# Obtained from: $NetBSD: Makefile,v 1.3 2002/03/05 21:57:00 thorpej Exp $

PROG=	semtest
NOMAN=

regress:
	@if ./semtest ./semtest; then \
		echo "PASS"; \
	else \
		echo "FAIL"; \
	fi

.include <bsd.prog.mk>
