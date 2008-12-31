# $FreeBSD: src/tools/regression/sysvshm/Makefile,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $
# SysV Shared Memory Regression Utility
# Obtained from: $NetBSD: Makefile,v 1.3 2002/03/05 21:57:00 thorpej Exp $

PROG=	shmtest
NO_MAN=

regress:
	@if ./shmtest ./shmtest; then \
		echo "PASS"; \
	else \
		echo "FAIL"; \
	fi

.include <bsd.prog.mk>
