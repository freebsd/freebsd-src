# $FreeBSD: src/tools/regression/sysvshm/Makefile,v 1.1 2002/08/15 06:34:37 alfred Exp $
# SysV Shared Memory Regression Utility
# Obtained from: $NetBSD: Makefile,v 1.3 2002/03/05 21:57:00 thorpej Exp $

PROG=	shmtest
NOMAN=	yes

regress:
	@if ./shmtest ./shmtest; then \
		echo "PASS"; \
	else \
		echo "FAIL"; \
	fi

.include <bsd.prog.mk>
